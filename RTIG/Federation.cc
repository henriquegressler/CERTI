// ----------------------------------------------------------------------------
// CERTI - HLA RunTime Infrastructure
// Copyright (C) 2002-2005  ONERA
//
// This file is part of CERTI
//
// CERTI is free software ; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation ; either version 2 of the License, or
// (at your option) any later version.
//
// CERTI is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY ; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program ; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
//
// ----------------------------------------------------------------------------

#include "Federation.hh"
#include <config.h>

// #include "NM_Classes.hh"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
// #include <ext/alloc_traits.h>
#include <iostream>
#include <list>
#include <memory>
#include <set>
#include <sys/stat.h>
#include <time.h>
#include <utility>

#include <libCERTI/AuditFile.hh>
#include <libCERTI/Interaction.hh>
#include <libCERTI/InteractionSet.hh>
#include <libCERTI/LBTS.hh>
#include <libCERTI/NM_Classes.hh>
#include <libCERTI/NetworkMessage.hh>
#include <libCERTI/Object.hh>
#include <libCERTI/ObjectAttribute.hh>
#include <libCERTI/ObjectClass.hh>
#include <libCERTI/ObjectClassAttribute.hh>
#include <libCERTI/ObjectClassSet.hh>
#include <libCERTI/ObjectSet.hh>
#include <libCERTI/PrettyDebug.hh>
#include <libCERTI/RootObject.hh>
#include <libCERTI/SecurityServer.hh>
#include <libCERTI/SocketTCP.hh>
#include <libCERTI/XmlParser.hh>
#include <libCERTI/XmlParser2000.hh>
#include <libCERTI/XmlParser2010.hh>
#include <libCERTI/fed.hh>

#include "MessageEvent.hh"

#include "make_unique.hh"

using std::pair;
using std::ifstream;
using std::ios;
using std::cout;
using std::endl;
using std::string;
using std::list;
using std::cerr;
using std::vector;

// Definitions
#ifdef HAVE_XML
// #include <libxml/xmlmemory.h>
#include "libxml/xmlstring.h"
#include <libxml/parser.h>
// #include <libxml/tree.h>
#define ROOT_NODE (const xmlChar*) "rtigSaveData"
#define NODE_FEDERATION (const xmlChar*) "federation"
#define NODE_FEDERATE (const xmlChar*) "federate"
#endif // HAVE_XML

#if defined(_WIN32) && !defined(__MINGW32__)
#define strcasecmp stricmp
#endif

namespace certi {

class RTIRegion;
class Socket;

namespace rtig {

static PrettyDebug D("FEDERATION", __FILE__);
static PrettyDebug G("GENDOC", __FILE__);
static PrettyDebug DNULL("RTIG_NULLMSG", "[RTIG NULL MSG]");

/**
 * \defgroup certi_FOM_FileSearch CERTI FOM file search algorithm
 * When a federate calls the CreateFederationExcution API
 * RTIG tries to open FOM file from different predefined places,
 * using various environment variables:
 *
 * -# Bare filename considered as a path provided through <code> FEDid_name </code>
 * -# Use CERTI federation object model search PATH
 *    <code>getenv(CERTI_FOM_PATH) + FEDid_name</code>.
 *    <br><code>CERTI_FOM_PATH</code> environment variable may contains a list of path
 *    separated with ':'.
 * -# Using the <code> CERTI_HOME </code> environment variable
 *    <code>getenv(CERTI_HOME)+"/share/federations/"+ FEDid_name</code>
 * -# installation place plus <code>FEDid_name</code>
 *    <br><code>PACKAGE_INSTALL_PREFIX + "/share/federation/" + FEDid_name</code>
 * -# on Unix <code>"/usr/local/share/federation/" + FEDid_name</code>
 *    for backward compatibility reason.
 */

#ifdef FEDERATION_USES_MULTICAST
Federation::Federation(const string& federation_name,
                       const FederationHandle federation_handle,
                       SocketServer& socket_server,
                       AuditFile& audit_server,
                       SocketMC* mc_link,
                       const int theVerboseLevel)
#else
Federation::Federation(const string& federation_name,
                       const FederationHandle federation_handle,
                       SocketServer& socket_server,
                       AuditFile& audit_server,
                       const string& FEDid_name,
                       const int verboseLevel)
#endif
    : my_handle(federation_handle), my_name(federation_name), my_FED_id(FEDid_name)
{
#ifdef FEDERATION_USES_MULTICAST // -----------------
    // Initialize Multicast
    if (mc_link == nullptr) {
        Debug(D, pdExcept) << "Null Multicast socket for new Federation." << endl;
        throw RTIinternalError("NULL Multicast socket for new Federation.");
    }

    Debug(D, pdInit) << "New Federation " << federation_handle << " will use Multicast." << endl;
    MCLink = mc_link;
#endif // FEDERATION_USES_MULTICAST // --------------

    Debug(G, pdGendoc) << "enter Federation::Federation" << endl;
    // Allocates Name
    if (federation_name.empty() || (!federation_handle.isValid())) {
        throw RTIinternalError("Null init parameter in Federation creation.");
    }

    Debug(D, pdInit) << "New Federation created with Handle " << my_handle << ", now reading FOM." << endl;

    // Initialize the Security Server.
    my_server = make_unique<SecurityServer>(socket_server, audit_server, my_handle);

    // Read FOM File to initialize Root Object.
    my_root_object = make_unique<RootObject>(my_server.get());

    if (verboseLevel > 0) {
        cout << "New federation: " << my_name << endl;
    }

    openFomFile(verboseLevel);

    my_min_NERx.setZero();

    enableMomIfAvailable();

    Debug(G, pdGendoc) << "exit Federation::Federation" << endl;
}

Federation::~Federation()
{
    Debug(D, pdInit) << "Destroying Federation " << my_handle << endl;

// If there are Federates, delete them all!
//     for (list<Federate *>::const_iterator i = begin(); i != end(); i++) {
//         delete(*i);
//     }
//     clear();

#ifdef FEDERATION_USES_MULTICAST
    FermerConnexion(mc_link);
    delete mc_link;
#endif
}

FederationHandle Federation::getHandle() const
{
    return my_handle;
}

string Federation::getName() const
{
    return my_name;
}

string Federation::getFEDid() const
{
    return my_FED_id;
}

int Federation::getNbFederates() const
{
    return my_federates.size();
}

int Federation::getNbRegulators() const
{
    return my_regulators.size();
}

bool Federation::isSynchronizing() const
{
    return !my_synchronization_labels.empty();
}

bool Federation::isMomEnabled() const
{
    return my_mom.operator bool();
}

bool Federation::isAutoProvideActive() const
{
    return my_auto_provide;
}

bool Federation::empty() const
{
    if (my_federates.empty()) {
        return true;
    }

    string reason{"<"};
    for (const auto& kv : my_federates) {
        reason += " " + kv.second->getName();
    }

    reason += " >";
    throw FederatesCurrentlyJoined(reason);
}

bool Federation::check(FederateHandle federate_handle) const
{
    if (my_mom && federate_handle == my_mom->getHandle()) {
        return false;
    }

    if (my_federates.count(federate_handle) == 0) {
        throw FederateNotExecutionMember(
            certi::stringize() << "Federate Handle <" << federate_handle << "> not found in federation <" << my_handle
                               << ">");
    }

    return true;
}

FederateHandle Federation::add(const string& federate_name, SocketTCP* tcp_link)
{
    try {
        getFederate(federate_name);
        throw FederateAlreadyExecutionMember("Federate with same name <" + federate_name
                                             + "> has already joined the federation");
    }
    catch (FederateNotExecutionMember& e) {
        // Nothing to do.
    }

    FederateHandle federate_handle = my_federate_handle_generator.provide();
    auto result
        = my_federates.insert(std::make_pair(federate_handle, make_unique<Federate>(federate_name, federate_handle)));

    Federate& federate = *result.first->second;

    Debug(D, pdInit) << "Federate " << federate_handle << " joined Federation " << my_handle << endl;

    // Send, to the newly added federate, a Null message from each regulating
    // federate (i) with their logical time h(i). This permits to calculate
    // its LBTS.
    NM_Message_Null nullMessage;
    NM_Announce_Synchronization_Point ASPMessage;
    try {
        vector<LBTS::FederateClock> clocks;
        my_regulators.get(clocks);

        for (const auto& clock : clocks) {
            nullMessage.setFederation(my_handle.get());
            nullMessage.setFederate(clock.first);
            nullMessage.setDate(clock.second);
            Debug(D, pdTerm) << "Sending NULL message from " << nullMessage.getFederate() << " to new federate."
                             << endl;

            nullMessage.send(tcp_link, my_nm_buffer);
        }

        // If federation is synchronizing, put federate in same state.
        if (isSynchronizing()) {
            ASPMessage.setFederate(federate_handle);
            ASPMessage.setFederation(my_handle.get());

            for (const auto& kv : my_synchronization_labels) {
                ASPMessage.setLabel(kv.first);
                ASPMessage.setTag(kv.second);
                Debug(D, pdTerm) << "Sending synchronization message " << kv.first << " to the new Federate" << endl;

                ASPMessage.send(tcp_link, my_nm_buffer);
                federate.addSynchronizationLabel(kv.first);
            }
        }
    }
    catch (NetworkError&) {
        throw RTIinternalError("Network Error while initializing federate.");
    }

    if (my_mom) {
        my_mom->registerFederate(federate);
        my_mom->updateFederatesInFederation();
    }

    return federate_handle;
}

// FIXME should return response
std::unique_ptr<NM_Resign_Federation_Execution> Federation::remove(FederateHandle federate_handle)
{
    //     HandleFederateMap::iterator i = _handleFederateMap.find(federate_handle);
    //     if (i != _handleFederateMap.end()) {
    //         // BUG: RemoveFederate: Should see if Federate owns attributes
    //         my_federate_handle_generator.free(federate_handle);
    //         _handleFederateMap.erase(i);
    //
    //         Debug(D, pdInit) << "Federation " << my_handle << ": Removed Federate " << federate_handle << endl;
    //         return;
    //     }
    //
    //     Debug(D, pdExcept) << "Federation " << my_handle << " could not remove unknown federate " << federate_handle
    //                        << endl;
    //     throw FederateNotExecutionMember(certi::stringize() << "Federate Handle=<" << federate_handle << ">");

    check(federate_handle);

    my_federate_handle_generator.free(federate_handle);

    my_federates.erase(my_federates.find(federate_handle));

    if (my_mom) {
        my_mom->deleteFederate(federate_handle);
        my_mom->updateFederatesInFederation();
    }

    auto rep = make_unique<NM_Resign_Federation_Execution>();

    rep->setFederate(federate_handle);
    rep->setFederation(my_handle.get());

    Debug(D, pdInit) << "Federation " << my_handle << ": Removed Federate " << federate_handle << endl;

    return rep;
}

void Federation::kill(FederateHandle federate_handle) noexcept
{
    Debug(D, pdInit) << "Killing Federate " << federate_handle << endl;

    try {
        auto& federate = getFederate(federate_handle);

        // is regulator ?
        try {
            removeRegulator(federate_handle);
            Debug(D, pdInit) << "Regulator Federate " << federate_handle << " removed" << endl;
        }
        catch (Exception& e) {
            Debug(D, pdInit) << "Federate " << federate_handle << " was not a regulator, continue" << endl;
        }

        // is constrained ?
        try {
            federate.setConstrained(false);

            Debug(D, pdInit) << "Constrained Federate " << federate_handle << " removed" << endl;
        }
        catch (Exception& e) {
            Debug(D, pdInit) << "Federate " << federate_handle << " was not constrained, continue" << endl;
        }

        // Remove references to this federate in root object
        my_root_object->killFederate(federate_handle);
        Debug(D, pdTrace) << "Federate " << federate_handle << " removed from the Root Object " << endl;

        // delete from federations list
        try {
            remove(federate_handle);
            Debug(D, pdInit) << "Federate " << federate_handle << " removed" << endl;
        }
        catch (Exception& e) {
        }
    }
    catch (FederateNotExecutionMember& e) {
        Debug(D, pdInit) << "Federate " << federate_handle << " was not from this federation" << endl;
    }
}

Responses Federation::addRegulator(FederateHandle federate_handle, FederationTime time)
{
    Responses responses;

    Federate& federate = getFederate(federate_handle);

    // It may throw RTIinternalError if Federate was regulator.
    my_regulators.insert(federate_handle, time);
    federate.setRegulator(true);

    Debug(D, pdTerm) << "Federation " << my_handle << ": Federate " << federate_handle
                     << " is now a regulator, Time=" << time.getTime() << endl;

    Debug(G, pdGendoc) << "      addRegulator====> write STR to all RTIA" << endl;
    auto msg = make_unique<NM_Set_Time_Regulating>();
    msg->setException(Exception::Type::NO_EXCEPTION);
    msg->setFederation(my_handle.get());
    msg->setFederate(federate_handle);
    msg->regulatorOn();
    msg->setDate(time);

    responses = respondToAll(std::move(msg));

    Debug(G, pdGendoc) << "      addRegulator====> write TRE to RTIA" << endl;
    auto rep = make_unique<NM_Time_Regulation_Enabled>();
    rep->setFederate(federate_handle);
    rep->setFederation(my_handle.get());
    rep->setDate(time);

    responses.emplace_back(my_server->getSocketLink(federate_handle), std::move(rep));

    if (my_mom) {
        my_mom->updateTimeRegulating(federate);
    }

    return responses;
}

void Federation::updateRegulator(FederateHandle federate_handle, FederationTime time, bool anonymous)
{
    // if it is an anonymous update (from NULL PRIME message), no need to check federate.
    if (!anonymous) {
        // It may throw FederateNotExecutionMember
        Federate& federate = getFederate(federate_handle);

        if (!federate.isRegulator()) {
            Debug(D, pdExcept) << "Federate " << federate_handle << " is not a regulator" << endl;
            throw RTIinternalError("Time regulation not enabled.");
        }

        Debug(D, pdDebug) << "Federation " << my_handle << ": Federate " << federate_handle << "'s new time is "
                          << time.getTime() << endl;
        my_regulators.update(federate_handle, time);
    }

    NM_Message_Null msg;
    msg.setFederation(my_handle.get());
    if (anonymous) {
        msg.setFederate(0);
    }
    else {
        msg.setFederate(federate_handle);
    }
    msg.setDate(time);
    DNULL.Out(pdDebug, "Snd NULL MSG (Federate=%d, Time = %f)", msg.getFederate(), msg.getDate().getTime());
    broadcastAnyMessage(&msg, federate_handle, anonymous);
}

Responses Federation::removeRegulator(FederateHandle federate_handle)
{
    Responses responses;

    Federate& federate = getFederate(federate_handle);

    // It may throw RTIinternalError if Federate was not regulator.
    my_regulators.remove(federate_handle);

    federate.setRegulator(false);

    Debug(D, pdTerm) << "Federation " << my_handle << ": Federate " << federate_handle << " is not a regulator anymore"
                     << endl;

    auto rep = make_unique<NM_Set_Time_Regulating>();
    rep->setFederate(federate_handle);
    rep->setFederation(my_handle.get());
    rep->regulatorOff();

    responses = respondToAll(std::move(rep));

    if (my_mom) {
        my_mom->updateTimeRegulating(federate);
    }

    return responses;
}

Responses Federation::setConstrained(FederateHandle federate_handle, bool constrained, FederationTime time)
{
    Responses responses;

    // It may throw FederateNotExecutionMember
    Federate& federate = getFederate(federate_handle);
    federate.setConstrained(constrained);

    if (constrained) {
        auto rep = make_unique<NM_Time_Constrained_Enabled>();
        rep->setFederate(federate_handle);
        rep->setFederation(my_handle.get());
        rep->setDate(time);

        responses.emplace_back(my_server->getSocketLink(federate_handle), std::move(rep));
    }

    if (my_mom) {
        my_mom->updateTimeConstrained(federate);
    }

    return responses;
}

void Federation::registerSynchronization(FederateHandle federate_handle, const string& label, const string& tag)
{
    Debug(G, pdGendoc) << "enter Federation::registerSynchronization for all federates" << endl;

    check(federate_handle);

    if (label.empty()) {
        throw RTIinternalError("Bad pause label(null).");
    }

    // If not already in pending labels, insert to list.
    auto result = my_synchronization_labels.insert(std::make_pair(label, tag));
    if (!result.second) {
        throw FederationAlreadyPaused("Label already pending");
    }

    // Add label to each federate (may throw RTIinternalError).
    for (const auto& kv : my_federates) {
        kv.second->addSynchronizationLabel(label);
    }

    Debug(D, pdTerm) << "Federation " << my_handle << " is now synchronizing for label " << label << endl;

    Debug(G, pdGendoc) << "exit  Federation::registerSynchronization for all federates" << endl;
}

void Federation::registerSynchronization(FederateHandle federate_handle,
                                         const string& label,
                                         const string& tag,
                                         const vector<FederateHandle>& federate_set)
{
    Debug(G, pdGendoc) << "enter Federation::registerSynchronization for federate set" << endl;

    Federate& federate = getFederate(federate_handle);

    if (label.empty()) {
        throw RTIinternalError("Bad pause label(null).");
    }

    // If not already in pending labels, insert to list.
    auto result = my_synchronization_labels.insert(pair<const string, string>(label, tag));
    if (!result.second) {
        throw FederationAlreadyPaused("Label already pending");
    }

    // FIXME This check is here to avoid regression, but there is a possible BUG here
    if (federate_set.size() != 0) {
        federate.addSynchronizationLabel(label);
    }

    for (auto& selectedFederate : federate_set) {
        my_federates.at(selectedFederate)->addSynchronizationLabel(label);
    }

    Debug(D, pdTerm) << "Federation " << my_handle << " is now synchronizing for label " << label << endl;

    Debug(G, pdGendoc) << "exit  Federation::registerSynchronization for federate set" << endl;
}

Responses Federation::unregisterSynchronization(FederateHandle federate_handle, const string& label)
{
    Responses responses;

    Debug(G, pdGendoc) << "enter Federation::unregisterSynchronization" << endl;

    check(federate_handle);

    if (label.empty()) {
        throw RTIinternalError("Bad pause label(null).");
    }

    // Set federate synchronized on this label.
    Federate& federate = getFederate(federate_handle);
    federate.removeSynchronizationLabel(label);

    // Test in every federate is synchronized. Otherwise, quit method.
    for (const auto& kv : my_federates) {
        if (kv.second->hasSynchronizationLabel(label)) {
            Debug(D, pdTerm) << "Federation " << my_handle << " is still paused on " << label << endl;

            Debug(G, pdGendoc) << "exit  Federation::unregisterSynchronization" << endl;
            return responses;
        }
    }

    // All federates from federation has called synchronizationPointAchieved.

    Debug(D, pdTerm) << "Federation " << my_handle << " is not Paused anymore" << endl;

    my_synchronization_labels.erase(label);

    // send a federationSynchronized().
    auto msg = make_unique<NM_Federation_Synchronized>();
    msg->setFederation(my_handle.get());
    msg->setFederate(federate_handle);
    msg->setLabel(label);

    responses = respondToAll(std::move(msg));

    Debug(D, pdTerm) << "Federation " << my_handle << " is synchronized on " << label << endl;

    Debug(G, pdGendoc) << "exit  Federation::unregisterSynchronization" << endl;

    return responses;
}

void Federation::broadcastSynchronization(FederateHandle federate_handle, const string& label, const string& tag)
{
    Debug(G, pdGendoc) << "enter Federation::broadcastSynchronization" << endl;

    check(federate_handle);

    if (label.empty()) {
        throw RTIinternalError("Bad pause label(null).");
    }

    // broadcast announceSynchronizationPoint() to all federates in federation.
    NM_Announce_Synchronization_Point msg;
    msg.setFederate(federate_handle);
    msg.setFederation(my_handle.get());
    msg.setLabel(label);
    msg.setTag(tag);

    Debug(G, pdGendoc) << "      broadcastSynchronization is calling broadcastAnyMessage for all federates" << endl;

    broadcastAnyMessage(&msg, 0, false);

    Debug(G, pdGendoc) << "exit  Federation::broadcastSynchronization" << endl;
}

void Federation::broadcastSynchronization(FederateHandle federate_handle,
                                          const string& label,
                                          const string& tag,
                                          const vector<FederateHandle>& federate_set)
{
    Debug(G, pdGendoc) << "enter Federation::broadcastSynchronization to some federates" << endl;

    check(federate_handle);

    if (label.empty()) {
        throw RTIinternalError("Bad pause label(null or too long).");
    }

    // broadcast announceSynchronizationPoint() to all federates in federation.
    NM_Announce_Synchronization_Point msg;
    msg.setFederate(federate_handle);
    msg.setFederation(my_handle.get());
    msg.setLabel(label);
    msg.setTag(tag);

    Debug(G, pdGendoc) << "      broadcastSynchronization is calling broadcastSomeMessage" << endl;

    broadcastSomeMessage(&msg, 0, federate_set, federate_set.size());

    Debug(G, pdGendoc) << "exit  Federation::broadcastSynchronization to some federates" << endl;
}

void Federation::requestFederationSave(FederateHandle federate_handle, const string& label, FederationTime time)
{
    Debug(G, pdGendoc) << "enter Federation::requestFederationSave with time" << endl;

    check(federate_handle);

    if (my_is_save_in_progress) {
        throw SaveInProgress("Already in saving state.");
    }

    for (const auto& kv : my_federates) {
        kv.second->setSaving(true);
        if (my_mom) {
            my_mom->updateFederateState(*kv.second);
        }
    }

    my_save_status = true;
    my_is_save_in_progress = true;
    my_save_label = label;

    NM_Initiate_Federate_Save msg;
    msg.setFederate(federate_handle);
    msg.setFederation(my_handle.get());
    msg.setLabel(label);
    // timed message
    msg.setDate(time);

    Debug(G, pdGendoc) << "      requestFederationSave====>broadcast I_F_S to all" << endl;

    broadcastAnyMessage(&msg, 0, false);

    if (my_mom) {
        my_mom->updateNextSave(label, time);
    }

    Debug(G, pdGendoc) << "exit  Federation::requestFederationSave with time" << endl;
}

void Federation::requestFederationSave(FederateHandle federate_handle, const string& the_label)
{
    Debug(G, pdGendoc) << "enter Federation::requestFederationSave without time" << endl;

    check(federate_handle);

    if (my_is_save_in_progress) {
        throw SaveInProgress("Already in saving state.");
    }

    //     for (HandleFederateMap::iterator j = _handleFederateMap.begin(); j != _handleFederateMap.end(); ++j) {
    for (const auto& kv : my_federates) {
        kv.second->setSaving(true);
        if (my_mom) {
            my_mom->updateFederateState(*kv.second);
        }
    }

    my_save_status = true;
    my_is_save_in_progress = true;
    my_save_label = the_label;

    NM_Initiate_Federate_Save msg;
    msg.setFederate(federate_handle);
    msg.setFederation(my_handle.get());
    msg.setLabel(the_label);

    Debug(G, pdGendoc) << "                  requestFederationSave====>broadcast I_F_S to all" << endl;

    broadcastAnyMessage(&msg, 0, false);

    if (my_mom) {
        my_mom->updateNextSave(the_label, 0);
    }

    Debug(G, pdGendoc) << "exit  Federation::requestFederationSave without time" << endl;
}

void Federation::federateSaveBegun(FederateHandle federate_handle)
{
    Debug(G, pdGendoc) << "enter Federation::federateSaveBegun" << endl;
    check(federate_handle);
    Debug(G, pdGendoc) << "exit  Federation::federateSaveBegun" << endl;
}

void Federation::federateSaveStatus(FederateHandle federate_handle, bool status)
{
    Debug(G, pdGendoc) << "enter Federation::federateSaveStatus" << endl;

    Federate& federate = getFederate(federate_handle);
    federate.setSaving(false);

    if (my_mom) {
        my_mom->updateFederateState(federate);
    }

    if (status == false) {
        my_save_status = false;
    }

    // Verify that all federates save ended (complete or not).
    for (const auto& kv : my_federates) {
        if (kv.second->isSaving()) {
            Debug(G, pdGendoc) << "exit  Federation::federateSaveStatus one federate has not save ended" << endl;
            return;
        }
    }

    // Save RTIG Data for future restoration.
    if (my_save_status) {
        my_save_status = saveXmlData();
    }

    // Send end save message.
    std::unique_ptr<NetworkMessage> msg(NM_Factory::create(
        my_save_status ? NetworkMessage::Type::FEDERATION_SAVED : NetworkMessage::Type::FEDERATION_NOT_SAVED));

    msg->setFederate(federate_handle);
    msg->setFederation(my_handle.get());

    broadcastAnyMessage(msg.get(), 0, false);

    Debug(G, pdGendoc) << "            =======> broadcast F_S or F_N_S" << endl;

    // Reinitialize state.
    my_save_status = true;
    my_is_save_in_progress = false;

    if (my_mom) {
        my_mom->updateLastSave();
        my_mom->updateNextSave("", 0);
    }

    Debug(G, pdGendoc) << "exit  Federation::federateSaveStatus" << endl;
}

void Federation::requestFederationRestore(FederateHandle federate_handle, const string& the_label)
{
    Debug(G, pdGendoc) << "enter Federation::requestFederationRestore" << endl;

    check(federate_handle);

    if (my_is_restore_in_progress) {
        throw RestoreInProgress("Already in restoring state.");
    }

// Informs sending federate of success/failure in restoring.
// At this point, only verify that file is present.
#ifdef HAVE_XML
    string filename = my_name + "_" + the_label + ".xcs";
    bool success = restoreXmlData(filename);
#else
    bool success = false;
#endif // HAVE_XML

    // JYR Note : forcing success to true to skip xmlParseFile (not compliant ?)
    success = true;

    NetworkMessage* msg = NM_Factory::create(success ? NetworkMessage::Type::REQUEST_FEDERATION_RESTORE_SUCCEEDED
                                                     : NetworkMessage::Type::REQUEST_FEDERATION_RESTORE_FAILED);

    msg->setFederate(federate_handle);
    msg->setFederation(my_handle.get());
    msg->setLabel(the_label);

    Socket* socket = my_server->getSocketLink(msg->getFederate());

    if (success) {
        Debug(G, pdGendoc) << "             =====> send message R_F_R_S to RTIA" << endl;
    }
    else {
        Debug(G, pdGendoc) << "             =====> send message R_F_R_F to RTIA" << endl;
    }

    msg->send(socket, my_nm_buffer);
    delete msg;

    // Reading file failed: not restoring !
    if (!success) {
        Debug(G, pdGendoc) << "exit  Federation::requestFederationRestore on success false" << endl;
        return;
    }

    // Otherwise...
    for (const auto& kv : my_federates) {
        kv.second->setRestoring(true);
        if (my_mom) {
            my_mom->updateFederateState(*kv.second);
        }
    }
    my_restore_status = true;
    my_is_restore_in_progress = true;

    // Informs federates a new restore is being done.
    msg = NM_Factory::create(NetworkMessage::Type::FEDERATION_RESTORE_BEGUN);
    msg->setFederate(federate_handle);
    msg->setFederation(my_handle.get());

    Debug(G, pdGendoc) << "             =====> broadcast message F_R_B" << endl;

    broadcastAnyMessage(msg, 0, false);
    delete msg;

    // For each federate, send an initiateFederateRestore with correct handle.
    msg = NM_Factory::create(NetworkMessage::Type::INITIATE_FEDERATE_RESTORE);
    msg->setFederation(my_handle.get());
    msg->setLabel(the_label);

    for (const auto& kv : my_federates) {
        msg->setFederate(kv.second->getHandle());

        // send message.
        socket = my_server->getSocketLink(kv.second->getHandle());
        Debug(G, pdGendoc) << "             =====> send message I_F_R to federate " << kv.second->getHandle() << endl;
        msg->send(socket, my_nm_buffer);
    }

    delete msg;
    Debug(G, pdGendoc) << "exit  Federation::requestFederationRestore" << endl;
}

void Federation::federateRestoreStatus(FederateHandle federate_handle, bool status)
{
    Debug(G, pdGendoc) << "enter Federation::federateRestoreStatus" << endl;
    Federate& federate = getFederate(federate_handle);
    federate.setRestoring(false);

    if (!status) {
        my_restore_status = false;
    }

    if (my_mom) {
        my_mom->updateFederateState(federate);
    }

    auto a_federate_is_still_restoring
        = std::any_of(begin(my_federates), end(my_federates), [](const decltype(my_federates)::value_type& kv) {
              return kv.second->isRestoring();
          });

    if (a_federate_is_still_restoring) {
        return;
    }

    // Send end restore message.
    std::unique_ptr<NetworkMessage> msg(NM_Factory::create(
        my_restore_status ? NetworkMessage::Type::FEDERATION_RESTORED : NetworkMessage::Type::FEDERATION_NOT_RESTORED));

    msg->setFederate(federate_handle);
    msg->setFederation(my_handle.get());

    broadcastAnyMessage(msg.get(), 0, false);

    // Reinitialize state.
    my_restore_status = true;
    my_is_restore_in_progress = false;
    Debug(G, pdGendoc) << "exit  Federation::federateRestoreStatus" << endl;
}

Responses Federation::publishObject(FederateHandle federate_handle,
                                    ObjectClassHandle object_handle,
                                    const vector<AttributeHandle>& attributes,
                                    bool publish_or_unpublish)
{
    Debug(G, pdGendoc) << "enter Federation::publishObject" << endl;

    Responses responses;

    check(federate_handle);

    // It may throw *NotDefined*
    my_root_object->ObjectClasses->publish(federate_handle, object_handle, attributes, publish_or_unpublish);

    if (attributes.size() != 0) { // do only for subscription
        // get object class from object class handle
        ObjectClass* objectClass = my_root_object->ObjectClasses->getObjectFromHandle(object_handle);

        // get publishers of attributes
        ObjectClassAttribute::PublishersList_t publishers;

        // get attributes of object class
        const ObjectClass::HandleClassAttributeMap& attributeMap = objectClass->getHandleClassAttributeMap();

        for (const auto& pair : attributeMap) {
            auto attributePublishers = pair.second->getPublishers();
            publishers.insert(begin(attributePublishers), end(attributePublishers));
        }

        // notify all publishers
        std::set<FederateHandle> federate_set;
        for (const auto& pub : publishers) {
            if (my_mom && pub == my_mom->getHandle()) {
                continue;
            }
            if (getFederate(pub).isClassRelevanceAdvisorySwitch()) {
                federate_set.insert(pub);
            }
        }

        auto msg = make_unique<NM_Start_Registration_For_Object_Class>();
        msg->setFederate(federate_handle);
        msg->setFederation(my_handle.get());
        msg->setObjectClass(object_handle);

        responses = respondToSome(std::move(msg), {begin(federate_set), end(federate_set)});
    }
    else { // unsubscribe branch
        // test if objectClass is subscribed by anyone else
        // -> yes : do nothing
        // -> no : test if publisher sets its CRA switch
        //    -> no : do nothing
        //    -> yes : inform publisher with federate service stopRegistrationForObjectClass
        //
    }

    if (publish_or_unpublish) {
        auto rep = make_unique<NM_Publish_Object_Class>();
        rep->setFederate(federate_handle);
        rep->setFederation(my_handle.get());
        rep->setObjectClass(object_handle);

        responses.emplace_back(my_server->getSocketLink(federate_handle), std::move(rep));
    }
    else {
        auto rep = make_unique<NM_Unpublish_Object_Class>();
        rep->setFederate(federate_handle);
        rep->setFederation(my_handle.get());
        rep->setObjectClass(object_handle);

        responses.emplace_back(my_server->getSocketLink(federate_handle), std::move(rep));
    }

    Debug(D, pdRegister) << "Federation " << my_handle << ": Federate " << federate_handle << "(un)publishes "
                         << attributes.size() << " attrib. of ObjClass " << object_handle << endl;
    Debug(G, pdGendoc) << "exit  Federation::publishObject" << endl;

    return responses;
}

Responses Federation::subscribeObject(FederateHandle federate,
                                      ObjectClassHandle object,
                                      const vector<AttributeHandle>& attributes,
                                      const bool subscribe_or_unsubscribe)
{
    Debug(G, pdGendoc) << "enter Federation::subscribeObject" << endl;

    Responses responses;

    check(federate);

    // It may throw AttributeNotDefined
    my_root_object->ObjectClasses->subscribe(federate, object, attributes);

    /*
     * The above code line (root->ObjectClasses->subscribe(...) calls the
     * subscription within the CERTI library in ObjectClassSet.cc. Further on,
     * ObjectClassSet::subscribe invokes ObjectClass::subscribe. That method
     * first unsubscribes all attributes, afterwards the subscription is
     * done in a for loop till list_size.
     * This means: Unsubscription and subscription are realized by the same
     * method. Only the list_size parameter controls the
     * unsubscription/subscription process.
     *
     * Do we need a cleaner solution, e.g. separate methods for subscription/
     * unsubscription?
     */

    if (attributes.size() != 0) { // do only for subscription
        // get object class from object class handle
        ObjectClass* objectClass = my_root_object->ObjectClasses->getObjectFromHandle(object);

        // get publishers of attributes
        ObjectClassAttribute::PublishersList_t publishers;

        // get attributes of object class
        const ObjectClass::HandleClassAttributeMap& attributeMap = objectClass->getHandleClassAttributeMap();

        for (const auto& pair : attributeMap) {
            auto attributePublishers = pair.second->getPublishers();
            publishers.insert(begin(attributePublishers), end(attributePublishers));
        }

        // notify all publishers
        std::set<FederateHandle> federate_set;
        for (const auto& pub : publishers) {
            if (my_mom && pub == my_mom->getHandle()) {
                continue;
            }
            if (getFederate(pub).isClassRelevanceAdvisorySwitch()) {
                federate_set.insert(pub);
            }
        }

        // broadcastSomeMessage needs a vector, no set -> conversion

        auto msg = make_unique<NM_Start_Registration_For_Object_Class>();
        msg->setFederate(federate);
        msg->setFederation(my_handle.get());
        msg->setObjectClass(object);

        responses = respondToSome(std::move(msg), {begin(federate_set), end(federate_set)});
    }
    else { // unsubscribe branch
        /* test if objectClass is subscribed by anyone else
         * -> yes : do nothing
         * -> no : test if publisher sets its CRA switch
         *  	-> no : do nothing
         *  	-> yes : inform publisher with federate service stopRegistrationForObjectClass
         */
    }

    if (subscribe_or_unsubscribe) {
        auto rep = make_unique<NM_Subscribe_Object_Class>();
        rep->setFederate(federate);
        rep->setObjectClass(object);

        responses.emplace_back(my_server->getSocketLink(federate), std::move(rep));
    }
    else {
        auto rep = make_unique<NM_Unsubscribe_Object_Class>();
        rep->setFederate(federate);
        rep->setObjectClass(object);

        responses.emplace_back(my_server->getSocketLink(federate), std::move(rep));
    }

    Debug(D, pdRegister) << "Federation " << my_handle << ": Federate " << federate << "(un)sub. to "
                         << attributes.size() << " attrib. of ObjClass " << object << endl;
    Debug(G, pdGendoc) << "exit  Federation::subscribeObject" << endl;

    return responses;
}

void Federation::reserveObjectInstanceName(FederateHandle theFederateHandle, string newObjName)
{
    Debug(G, pdGendoc) << "enter Federation::reserveObjectInstanceName" << endl;
    Socket* socket;
    NetworkMessage* msg;

    bool reservation_ok = my_root_object->reserveObjectInstanceName(theFederateHandle, newObjName);

    if (my_mom && theFederateHandle == my_mom->getHandle()) {
        Debug(G, pdGendoc) << "             =====> Object instance name reservation for MOM: "
                           << (reservation_ok ? "Success" : "Failure") << endl;
        Debug(G, pdGendoc) << "exit  Federation::reserveObjectInstanceName" << endl;
        return;
    }

    if (reservation_ok) {
        msg = NM_Factory::create(NetworkMessage::Type::RESERVE_OBJECT_INSTANCE_NAME_SUCCEEDED);
        NM_Reserve_Object_Instance_Name_Succeeded* okMsg
            = dynamic_cast<NM_Reserve_Object_Instance_Name_Succeeded*>(msg);

        okMsg->setObjectName(newObjName);
        Debug(G, pdGendoc) << "             =====> send message R_O_I_N_S to federate " << msg->getFederate() << endl;
    }
    else {
        msg = NM_Factory::create(NetworkMessage::Type::RESERVE_OBJECT_INSTANCE_NAME_FAILED);
        NM_Reserve_Object_Instance_Name_Failed* nokMsg = dynamic_cast<NM_Reserve_Object_Instance_Name_Failed*>(msg);

        nokMsg->setObjectName(newObjName);
        Debug(G, pdGendoc) << "             =====> send message R_O_I_N_F to federate " << msg->getFederate() << endl;
    }

    msg->setFederation(my_handle.get());
    msg->setFederate(theFederateHandle);
    // send message.
    socket = my_server->getSocketLink(msg->getFederate());
    msg->send(socket, my_nm_buffer);

    delete msg;
    Debug(G, pdGendoc) << "exit  Federation::reserveObjectInstanceName" << endl;
}

ObjectHandle
Federation::registerObject(FederateHandle federate, ObjectClassHandle class_handle, const string& object_name)
{
    ObjectHandle id = my_objects_handle_generator.provide();

    Debug(G, pdGendoc) << "enter Federation::registerObject" << endl;
    Debug(D, pdRegister) << "Federation " << my_handle << ": Federate " << federate << " registering Object " << id
                         << " of Class " << class_handle << endl;

    string strname;
    if (!object_name.empty()) {
        strname = object_name;
    }
    else {
        // create a name if necessary
        strname = "HLAObject_" + std::to_string(id);
    }

    // Register Object.
    try {
        my_root_object->registerObjectInstance(federate, class_handle, id, strname);
    }
    catch (...) { //If an exception was thrown, the object instance was not added
        //and we can free the object handle id for future use and rethrow
        //the exception
        my_objects_handle_generator.free(id);
        throw;
    }
    Debug(G, pdGendoc) << "exit Federation::registerObject" << endl;
    return id;
}

Responses Federation::deleteObject(FederateHandle federate_handle,
                                   ObjectHandle object_handle,
                                   FederationTime time,
                                   const string& tag)
{
    Responses responses;

    check(federate_handle);

    Debug(D, pdRegister) << "Federation " << my_handle << ": Federate " << federate_handle << " destroys object "
                         << object_handle << endl;

    my_root_object->deleteObjectInstance(federate_handle, object_handle, time, tag);
    my_objects_handle_generator.free(object_handle);

    auto rep = make_unique<NM_Delete_Object>();
    rep->setFederate(federate_handle);
    rep->setObject(object_handle);

    responses.emplace_back(my_server->getSocketLink(federate_handle), std::move(rep));

    return responses;
}

Responses Federation::deleteObject(FederateHandle federate_handle, ObjectHandle object_handle, const string& tag)
{
    Responses responses;

    check(federate_handle);

    Debug(D, pdRegister) << "Federation " << my_handle << ": Federate " << federate_handle << " destroys object "
                         << object_handle << endl;

    my_root_object->deleteObjectInstance(federate_handle, object_handle, tag);
    my_objects_handle_generator.free(object_handle);

    auto rep = make_unique<NM_Delete_Object>();
    rep->setFederate(federate_handle);
    rep->setObject(object_handle);

    responses.emplace_back(my_server->getSocketLink(federate_handle), std::move(rep));

    return responses;
}

FederateHandle Federation::requestObjectOwner(FederateHandle theFederateHandle,
                                              ObjectHandle theObject,
                                              const vector<AttributeHandle>& theAttributeList)
{
    Debug(G, pdGendoc) << "enter Federation::requestObjectOwner" << endl;

    // Find owner of each attribute and store them by owner
    std::map<FederateHandle, vector<AttributeHandle>> attributesForFederates;
    Object* actualObject = my_root_object->getObject(theObject);

    for (const auto& attribute : theAttributeList) {
        FederateHandle federateHandle = actualObject->getAttribute(attribute)->getOwner();

        // Only attributes that are owned by someone should be asked
        if (federateHandle != 0) {
            auto existingFederate = attributesForFederates.find(federateHandle);
            if (existingFederate == end(attributesForFederates)) {
                existingFederate
                    = attributesForFederates.insert(std::make_pair(federateHandle, vector<AttributeHandle>())).first;
            }

            existingFederate->second.push_back(attribute);
        }
    }

    // Send a PAVU to each owner with the list of attributes
    for (const auto& pair : attributesForFederates) {
        if (my_mom && pair.first == my_mom->getHandle()) {
            my_mom->provideAttributeValueUpdate(theObject, pair.second);
            continue;
        }

        NM_Provide_Attribute_Value_Update message;

        // Send a PROVIDE_ATTRIBUTE_VALUE_UPDATE to the owner
        message.setFederate(theFederateHandle);
        message.setObject(theObject);
        message.setAttributesSize(pair.second.size());
        auto i = 0u;
        for (const auto& attribute : pair.second) {
            message.setAttributes(attribute, ++i);
        }

        message.send(my_server->getSocketLink(pair.first), my_nm_buffer);
    }

    // Request Object.
    FederateHandle theOwnerHandle = my_root_object->requestObjectOwner(theFederateHandle, theObject);

    Debug(G, pdGendoc) << "            requestObjectOwner ===> write PAVU to RTIA " << theOwnerHandle << endl;
    Debug(G, pdGendoc) << "exit  Federation::requestObjectOwner" << endl;

    return theOwnerHandle;
}

void Federation::requestClassAttributeValueUpdate(FederateHandle theFederateHandle,
                                                  ObjectClassHandle theClassHandle,
                                                  const vector<AttributeHandle>& theAttributeList)
{
    Debug(G, pdGendoc) << "enter Federation::requestClassAttributeValueUpdate" << endl;

    // get object class
    ObjectClass* oClass = my_root_object->getObjectClass(theClassHandle);
    if (!oClass) {
        throw ObjectClassNotDefined("ObjectClassHandle <" + std::to_string(theClassHandle) + "> is unknown.");
    }

    // send PAVU for all objects of this class
    ObjectClass::HandleObjectMap instances = oClass->getClassInstances();
    for (ObjectClass::HandleObjectMap::const_iterator it = instances.begin(); it != instances.end(); ++it) {
        requestObjectOwner(theFederateHandle, it->first, theAttributeList);
    }
    Debug(G, pdGendoc) << "exit  Federation::requestClassAttributeValueUpdate" << endl;
}

void Federation::updateAttributeValues(FederateHandle federate,
                                       ObjectHandle object_handle,
                                       const vector<AttributeHandle>& attributes,
                                       const vector<AttributeValue_t>& values,
                                       FederationTime time,
                                       const string& tag)
{
    Debug(G, pdGendoc) << "enter Federation::updateAttributeValues with time" << endl;
    check(federate);

    // Get the object pointer by id from the root object
    Object* object = my_root_object->objects->getObject(object_handle);

    // It may throw *NotDefined
    my_root_object->ObjectClasses->updateAttributeValues(federate, object, attributes, values, time, tag);

    if (my_mom) {
        my_mom->registerObjectInstanceUpdated(federate, object->getClass(), object_handle);
    }

    Debug(D, pdRegister) << "Federation " << my_handle << ": Federate " << federate << " updated attributes of Object "
                         << object_handle << endl;
    Debug(G, pdGendoc) << "exit  Federation::updateAttributeValues with time" << endl;
}

void Federation::updateAttributeValues(FederateHandle federate,
                                       ObjectHandle object_handle,
                                       const vector<AttributeHandle>& attributes,
                                       const vector<AttributeValue_t>& values,
                                       const string& tag)
{
    Debug(G, pdGendoc) << "enter Federation::updateAttributeValues without time" << endl;
    check(federate);

    // Get the object pointer by id from the root object
    Object* object = my_root_object->objects->getObject(object_handle);

    // It may throw *NotDefined
    my_root_object->ObjectClasses->updateAttributeValues(federate, object, attributes, values, tag);

    if (my_mom) {
        my_mom->registerObjectInstanceUpdated(federate, object->getClass(), object_handle);
        my_mom->registerUpdate(federate, object->getClass());
        my_mom->updateUpdatesSent(federate);
    }

    Debug(D, pdRegister) << "Federation " << my_handle << ": Federate " << federate << " updated attributes of Object "
                         << object_handle << endl;
    Debug(G, pdGendoc) << "exit  Federation::updateAttributeValues without time" << endl;
}

Responses Federation::publishInteraction(FederateHandle federate_handle,
                                         InteractionClassHandle interaction_class_handle,
                                         bool publish_or_unpublish)
{
    Responses responses;

    check(federate_handle);

    // It may throw InteractionClassNotDefined
    my_root_object->Interactions->publish(federate_handle, interaction_class_handle, publish_or_unpublish);
    Debug(D, pdRequest) << "Federation " << my_handle << ": Federate " << federate_handle
                        << " has(un)published Interaction " << interaction_class_handle << endl;

    if (publish_or_unpublish) {
        auto rep = make_unique<NM_Publish_Interaction_Class>();
        rep->setFederate(federate_handle);
        rep->setInteractionClass(interaction_class_handle);

        responses.emplace_back(my_server->getSocketLink(federate_handle), std::move(rep));
    }
    else {
        auto rep = make_unique<NM_Unpublish_Interaction_Class>();
        rep->setFederate(federate_handle);
        rep->setInteractionClass(interaction_class_handle);

        responses.emplace_back(my_server->getSocketLink(federate_handle), std::move(rep));
    }

    return responses;
}

Responses Federation::subscribeInteraction(FederateHandle federate_handle,
                                           InteractionClassHandle interaction_class_handle,
                                           bool subscribe_or_unsubscribe)
{
    Responses responses;

    check(federate_handle);

    // It may throw *NotDefined
    my_root_object->Interactions->subscribe(federate_handle, interaction_class_handle, 0, subscribe_or_unsubscribe);
    Debug(D, pdRegister) << "Federation " << my_handle << ": Federate " << federate_handle
                         << "(un)subscribes to Interaction " << interaction_class_handle << endl;

    if (subscribe_or_unsubscribe) {
        auto rep = make_unique<NM_Subscribe_Interaction_Class>();
        rep->setFederate(federate_handle);
        rep->setInteractionClass(interaction_class_handle);

        responses.emplace_back(my_server->getSocketLink(federate_handle), std::move(rep));
    }
    else {
        auto rep = make_unique<NM_Unsubscribe_Interaction_Class>();
        rep->setFederate(federate_handle);
        rep->setInteractionClass(interaction_class_handle);

        responses.emplace_back(my_server->getSocketLink(federate_handle), std::move(rep));
    }

    return responses;
}

Responses Federation::broadcastInteraction(FederateHandle federate_handle,
                                           InteractionClassHandle interaction_class_handle,
                                           const vector<ParameterHandle>& parameter_handles,
                                           const vector<ParameterValue_t>& parameter_values,
                                           FederationTime time,
                                           RegionHandle region_handle,
                                           const string& tag)
{
    Debug(G, pdGendoc) << "enter Federation::broadcastInteraction with time" << endl;

    Responses responses;

    check(federate_handle);

    const RTIRegion* region = 0;
    if (region_handle != 0) {
        region = my_root_object->getRegion(region_handle);
    }

    my_root_object->Interactions->broadcastInteraction(federate_handle,
                                                       interaction_class_handle,
                                                       parameter_handles,
                                                       parameter_values,
                                                       parameter_handles.size(),
                                                       time,
                                                       region,
                                                       tag);
    Debug(D, pdRequest) << "Federation " << my_handle << ": Broadcasted Interaction " << interaction_class_handle
                        << " from Federate " << federate_handle << " nb params " << parameter_handles.size() << endl;

    auto rep = make_unique<NM_Send_Interaction>();
    rep->setFederate(federate_handle);
    rep->setInteractionClass(interaction_class_handle);
    rep->setTag(tag);

    responses.emplace_back(my_server->getSocketLink(federate_handle), std::move(rep));

    if (my_mom) {
        my_mom->registerInteractionSent(federate_handle, interaction_class_handle);
        my_mom->updateInteractionsSent(federate_handle);

        if (my_root_object->Interactions->getObjectFromHandle(interaction_class_handle)
                ->isSubscribed(my_mom->getHandle())) {
            auto mom_responses = my_mom->processInteraction(
                /*federate_handle, */ interaction_class_handle, parameter_handles, parameter_values, region_handle);
            responses.insert(
                end(responses), make_move_iterator(begin(mom_responses)), make_move_iterator(end(mom_responses)));
        }
    }

    Debug(G, pdGendoc) << "exit Federation::broadcastInteraction with time" << endl;

    return responses;
}

Responses Federation::broadcastInteraction(FederateHandle federate_handle,
                                           InteractionClassHandle interaction_class_handle,
                                           const vector<ParameterHandle>& parameter_handles,
                                           const vector<ParameterValue_t>& parameter_values,
                                           RegionHandle region_handle,
                                           const string& tag)
{
    Debug(G, pdGendoc) << "enter Federation::broadcastInteraction without time" << endl;

    Responses responses;

    check(federate_handle);

    const RTIRegion* region{nullptr};
    if (region_handle != 0) {
        region = my_root_object->getRegion(region_handle);
    }

    my_root_object->Interactions->broadcastInteraction(federate_handle,
                                                       interaction_class_handle,
                                                       parameter_handles,
                                                       parameter_values,
                                                       parameter_handles.size(),
                                                       region,
                                                       tag);
    Debug(D, pdRequest) << "Federation " << my_handle << ": Broadcasted Interaction " << interaction_class_handle
                        << " from Federate " << federate_handle << " nb params " << parameter_handles.size() << endl;
    for (size_t i{0u}; i < parameter_handles.size(); i++) {
        Debug(D, pdRequest) << " Param " << parameter_handles[i] << " Value "
                            << string(&(parameter_values[i][0]), parameter_values[i].size()) << endl;
    }

    auto rep = make_unique<NM_Send_Interaction>();
    rep->setFederate(federate_handle);
    rep->setInteractionClass(interaction_class_handle);
    rep->setTag(tag);

    responses.emplace_back(my_server->getSocketLink(federate_handle), std::move(rep));

    if (my_mom) {
        my_mom->registerInteractionSent(federate_handle, interaction_class_handle);
        my_mom->updateInteractionsSent(federate_handle);

        if (my_root_object->Interactions->getObjectFromHandle(interaction_class_handle)
                ->isSubscribed(my_mom->getHandle())) {
            auto mom_responses = my_mom->processInteraction(
                /*federate_handle, */ interaction_class_handle, parameter_handles, parameter_values, region_handle);
            responses.insert(
                end(responses), make_move_iterator(begin(mom_responses)), make_move_iterator(end(mom_responses)));
        }
    }

    Debug(G, pdGendoc) << "exit Federation::broadcastInteraction without time" << endl;

    return responses;
}

bool Federation::isOwner(FederateHandle federate_handle, ObjectHandle object_handle, AttributeHandle attribute_handle)
{
    check(federate_handle);

    Debug(D, pdDebug) << "Owner of Object " << object_handle << " Atrribute " << attribute_handle << endl;

    // It may throw *NotDefined
    return my_root_object->objects->isAttributeOwnedByFederate(federate_handle, object_handle, attribute_handle);
}

void Federation::queryAttributeOwnership(FederateHandle federate_handle,
                                         ObjectHandle object_handle,
                                         AttributeHandle attribute_handle)
{
    check(federate_handle);

    Debug(D, pdDebug) << "Owner of Object " << object_handle << " Atrribute " << attribute_handle << endl;

    // It may throw *NotDefined
    my_root_object->objects->queryAttributeOwnership(federate_handle, object_handle, attribute_handle);
}

void Federation::negotiateDivestiture(FederateHandle federate_handle,
                                      ObjectHandle object_handle,
                                      const vector<AttributeHandle>& attribs,
                                      const string& tag)
{
    check(federate_handle);

    // Get the object pointer by id from the root object
    Object* object = my_root_object->objects->getObject(object_handle);

    // It may throw *NotDefined
    my_root_object->ObjectClasses->negotiatedAttributeOwnershipDivestiture(federate_handle, object, attribs, tag);
}

void Federation::acquireIfAvailable(FederateHandle federate_handle,
                                    ObjectHandle object_handle,
                                    const vector<AttributeHandle>& attribs)
{
    check(federate_handle);

    // Get the object pointer by id from the root object
    Object* object = my_root_object->objects->getObject(object_handle);

    // It may throw *NotDefined
    my_root_object->ObjectClasses->attributeOwnershipAcquisitionIfAvailable(federate_handle, object, attribs);
}

Responses
Federation::divest(FederateHandle federate_handle, ObjectHandle object_handle, const vector<AttributeHandle>& attrs)
{
    Responses responses;

    check(federate_handle);

    // Get the object pointer by id from the root object
    Object* object = my_root_object->objects->getObject(object_handle);

    // It may throw *NotDefined
    my_root_object->ObjectClasses->unconditionalAttributeOwnershipDivestiture(federate_handle, object, attrs);

    auto rep = make_unique<NM_Unconditional_Attribute_Ownership_Divestiture>();
    rep->setFederate(federate_handle);
    rep->setObject(object_handle);

    responses.emplace_back(my_server->getSocketLink(federate_handle), std::move(rep));

    return responses;
}

void Federation::acquire(FederateHandle federate_handle,
                         ObjectHandle object_handle,
                         const vector<AttributeHandle>& attributes,
                         const string& tag)
{
    check(federate_handle);

    // Get the object pointer by id from the root object
    Object* object = my_root_object->objects->getObject(object_handle);

    // It may throw *NotDefined
    my_root_object->ObjectClasses->attributeOwnershipAcquisition(federate_handle, object, attributes, tag);

    Debug(D, pdDebug) << "Acquisition on Object " << object_handle << endl;
}

void Federation::cancelDivestiture(FederateHandle federate_handle,
                                   ObjectHandle id,
                                   const vector<AttributeHandle>& attributes)
{
    check(federate_handle);

    // It may throw *NotDefined
    my_root_object->objects->cancelNegotiatedAttributeOwnershipDivestiture(
        federate_handle, id, attributes, attributes.size());

    Debug(D, pdDebug) << "CancelDivestiture sur Objet " << id << endl;
}

AttributeHandleSet* Federation::respondRelease(FederateHandle federate_handle,
                                               ObjectHandle object_handle,
                                               const vector<AttributeHandle>& attributes)
{
    check(federate_handle);

    Debug(D, pdDebug) << "RespondRelease on Object " << object_handle << endl;

    // Get the object pointer by id from the root object
    Object* object = my_root_object->objects->getObject(object_handle);

    // It may throw *NotDefined
    return my_root_object->ObjectClasses->attributeOwnershipReleaseResponse(federate_handle, object, attributes);
}

void Federation::cancelAcquisition(FederateHandle federate_handle,
                                   ObjectHandle object_handle,
                                   const vector<AttributeHandle>& attributes)
{
    check(federate_handle);

    Debug(D, pdDebug) << "CancelAcquisition sur Objet " << object_handle << endl;

    // Get the object pointer by id from the root object
    Object* object = my_root_object->objects->getObject(object_handle);

    // It may throw *NotDefined
    my_root_object->ObjectClasses->cancelAttributeOwnershipAcquisition(federate_handle, object, attributes);
}

long Federation::createRegion(FederateHandle federate_handle, SpaceHandle space_handle, long extents_count)
{
    check(federate_handle);

    return my_root_object->createRegion(space_handle, extents_count);
}

void Federation::modifyRegion(FederateHandle federate_handle, RegionHandle region, const vector<Extent>& extents)
{
    check(federate_handle);
    my_root_object->modifyRegion(region, extents);
}

void Federation::deleteRegion(FederateHandle federate_handle, long region)
{
    check(federate_handle);

    if (my_is_save_in_progress) {
        throw SaveInProgress("");
    }
    if (my_is_restore_in_progress) {
        throw RestoreInProgress("");
    }

    // TODO: check RegionInUse
    my_root_object->deleteRegion(region);
}

void Federation::associateRegion(FederateHandle federate_handle,
                                 ObjectHandle object,
                                 RegionHandle the_handle,
                                 const vector<AttributeHandle>& attributes)
{
    check(federate_handle);

    RTIRegion* region = my_root_object->getRegion(the_handle);

    my_root_object->getObject(object)->unassociate(region);

    for (size_t i{0u}; i < attributes.size(); ++i) {
        my_root_object->getObjectAttribute(object, attributes[i])->associate(region);
    }
}

void Federation::unassociateRegion(FederateHandle federate_handle, ObjectHandle object, RegionHandle the_handle)
{
    check(federate_handle);

    RTIRegion* region = my_root_object->getRegion(the_handle);
    my_root_object->getObject(object)->unassociate(region);
}

void Federation::subscribeAttributesWR(FederateHandle federate_handle,
                                       ObjectClassHandle c,
                                       RegionHandle region_handle,
                                       const vector<AttributeHandle>& attributes)
{
    check(federate_handle);
    my_root_object->ObjectClasses->subscribe(federate_handle, c, attributes, my_root_object->getRegion(region_handle));
}

void Federation::unsubscribeAttributesWR(FederateHandle federate_handle,
                                         ObjectClassHandle object_class_handle,
                                         RegionHandle region_handle)
{
    check(federate_handle);

    RTIRegion* region = my_root_object->getRegion(region_handle);

    my_root_object->getObjectClass(object_class_handle)->unsubscribe(federate_handle, region);
}

void Federation::subscribeInteractionWR(FederateHandle federate_handle,
                                        InteractionClassHandle interaction_class_handle,
                                        RegionHandle region_handle)
{
    check(federate_handle);

    RTIRegion* region = my_root_object->getRegion(region_handle);

    my_root_object->getInteractionClass(interaction_class_handle)->subscribe(federate_handle, region);
}

void Federation::unsubscribeInteractionWR(FederateHandle federate_handle,
                                          InteractionClassHandle interaction_class_handle,
                                          RegionHandle region_handle)
{
    check(federate_handle);

    RTIRegion* region = my_root_object->getRegion(region_handle);

    my_root_object->getInteractionClass(interaction_class_handle)->unsubscribe(federate_handle, region);
}

ObjectHandle Federation::registerObjectWithRegion(FederateHandle federate_handle,
                                                  ObjectClassHandle class_handle,
                                                  const string& object_name,
                                                  RegionHandle region_handle,
                                                  const vector<AttributeHandle>& attributes)
{
    Debug(G, pdGendoc) << "enter Federation::registerObjectWithRegion" << endl;
    check(federate_handle);

    // Register object
    ObjectHandle object = my_objects_handle_generator.provide();
    Debug(D, pdDebug) << "Register object with region : Object " << object << ", class " << class_handle << ", region "
                      << region_handle << endl;
    string strname;
    if (!object_name.empty()) {
        strname = object_name;
    }
    else {
        // create a name if necessary
        strname = "HLAObject_" + std::to_string(object);
    }

    my_root_object->registerObjectInstance(federate_handle, class_handle, object, strname);

    Debug(D, pdDebug) << "- object \"" << strname << "\" registered" << endl;

    // Associate region
    RTIRegion* region = my_root_object->getRegion(region_handle);
    my_root_object->getObject(object)->unassociate(region);

    for (size_t i{0u}; i < attributes.size(); ++i) {
        my_root_object->getObjectAttribute(object, attributes[i])->associate(region);
    }

    Debug(D, pdDebug) << "- " << attributes.size() << " attribute(s) associated with region " << region_handle << endl;
    Debug(G, pdGendoc) << "exit  Federation::registerObjectWithRegion" << endl;
    return object;
}

void Federation::getFOM(NM_Join_Federation_Execution& object_model_data)
{
    my_root_object->convertToSerializedFOM(object_model_data);
}

bool Federation::updateLastNERxForFederate(FederateHandle federate_handle, FederationTime date)
{
    bool retval = false;
    FederationTime newMin;
    Federate& f = getFederate(federate_handle);

    f.setLastNERxValue(date);
    Debug(D, pdDebug) << "Federate <" << f.getName() << "> has new NERx value=" << date.getTime() << endl;
    newMin = computeMinNERx();
    if (newMin > my_min_NERx) {
        Debug(D, pdDebug) << "New minNERx =" << newMin << endl;
        retval = true;
        my_min_NERx = newMin;
        /* if a new Min is found then we shall reset the NERx status of ALL NERing federates
		 * Rationale: an Anonymous NULL MESSAGE dated with newMin will be sent to them.
		 *            after that a NERing Federate which remains in its NERing loop should
		 *            send a new NULL PRIME message.
		 */
        //         for (HandleFederateMap::iterator i = _handleFederateMap.begin(); i != _handleFederateMap.end(); ++i) {
        for (const auto& kv : my_federates) {
            if (kv.second->isUsingNERx()) {
                //federate_handle.setLastNERxValue(FedTime(0.0)); // not needed
                kv.second->setIsUsingNERx(false);
                Debug(D, pdDebug) << "Federate <" << kv.second->getName() << "> not NERing anymore." << endl;
            }
        }
    }
    return retval;
}

FederationTime Federation::computeMinNERx()
{
    FederationTime retval;
    uint32_t nbFed;
    LBTS NER_regulators;
    vector<LBTS::FederateClock> clocks;
    retval.setZero();
    nbFed = 0;
    my_regulators.get(clocks);

    /* Build a set of clocks */
    for (const auto& clock : clocks) {
        FederateHandle h = clock.first;
        //         HandleFederateMap::iterator f = federate.find(h);
        Federate& f = getFederate(h);
        if (f.isUsingNERx()) {
            nbFed++;
            NER_regulators.insert(h, f.getLastNERxValue());
        }
        else {
            NER_regulators.insert(h, clock.second);
        }
    }

    /* compute the new NERx LBTS (minimum) */
    NER_regulators.compute();
    retval = NER_regulators.getLBTSValue();
    Debug(D, pdDebug) << "MinNERx =" << retval.getTime() << endl;

    /* the minimum is different from 0 iff more than 2 federate use NERx */
    if (nbFed < 2) {
        retval.setZero();
    }

    Debug(D, pdDebug) << "computeMinNERx =" << retval.getTime() << endl;
    return retval;
}

FederationTime Federation::getMinNERx() const
{
    return my_min_NERx;
}

Federate& Federation::getFederate(FederateHandle federate_handle)
{
    try {
        return *my_federates.at(federate_handle);
    }
    catch (std::out_of_range& e) {
        throw FederateNotExecutionMember("Federate Handle <" + std::to_string(federate_handle) + "> not found.");
    }
}

Federate& Federation::getFederate(const string& federate_name)
{
    auto it = std::find_if(
        begin(my_federates), end(my_federates), [&federate_name](decltype(my_federates)::value_type& kv) {
            return kv.second->getName() == federate_name;
        });

    if (it == end(my_federates)) {
        throw FederateNotExecutionMember(certi::stringize() << "Federate <" << federate_name << "> not found.");
    }

    return *it->second;
}

// Path splitting functions
vector<string>& split(const string& s, char delim, vector<string>& elems)
{
    std::stringstream ss(s);
    string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

vector<string> split(const string& s, char delim)
{
    vector<string> elems;
    return split(s, delim, elems);
}

void Federation::openFomFile(const int verboseLevel)
{
    // We should try to open FOM file from different
    // predefined places:
    // --> see doxygen doc at the top of this file.
    string filename = my_FED_id;
    bool filefound = false;

    if (verboseLevel > 0) {
        cout << "Looking for FOM file... " << endl;
        cout << "   Trying... " << filename;
    }

    STAT_STRUCT file_stat;
    filefound = (0 == STAT_FUNCTION(filename.c_str(), &file_stat));

    /* This is the main path handling loop */
    if (!filefound) {
        vector<string> fom_paths;
#ifdef WIN32
        char temp[260];
        GetCurrentDirectory(260, temp);
        fom_paths.insert(fom_paths.end(), string(temp) + "\\share\\federations\\");
#endif

        /* add paths from CERTI_FOM_PATH */
        if (NULL != getenv("CERTI_FOM_PATH")) {
            string path = getenv("CERTI_FOM_PATH");
            vector<string> certi_fom_paths = split(path, ':');
            fom_paths.insert(fom_paths.end(), certi_fom_paths.begin(), certi_fom_paths.end());
        }

        if (NULL != getenv("CERTI_HOME")) {
#ifdef WIN32
            fom_paths.insert(fom_paths.end(), string(getenv("CERTI_HOME")) + "\\share\\federations\\");
#else
            fom_paths.insert(fom_paths.end(), string(getenv("CERTI_HOME")) + "/share/federations/");
#endif
        }

#ifdef WIN32
        fom_paths.insert(fom_paths.end(), PACKAGE_INSTALL_PREFIX "\\share\\federations\\");
#else
        fom_paths.insert(fom_paths.end(), PACKAGE_INSTALL_PREFIX "/share/federations/");
        fom_paths.insert(fom_paths.end(), "/usr/local/share/federations/");
#endif

        /* try to open FED using fom_paths prefixes */
        for (const string& path : fom_paths) {
            if (verboseLevel > 0) {
                cout << " --> cannot access." << endl;
            }
            filename = path + my_FED_id;
            if (verboseLevel > 0) {
                cout << "   Now trying... " << filename;
            }
            filefound = (0 == STAT_FUNCTION(filename.c_str(), &file_stat));
            if (filefound) {
                break;
            }
        }
    }

    if (!filefound) {
        if (verboseLevel > 0) {
            cout << " --> cannot access." << endl;
        }
        cerr << "Next step will fail, abort now" << endl;
        Debug(G, pdGendoc) << "exit Federation::Federation on exception CouldNotOpenFED" << endl;
        throw CouldNotOpenFED("RTIG cannot find FED file.");
    }

    // now really assign FEDid
    my_FED_id = filename;

    // Try to open to verify if file exists
    ifstream fedTry(my_FED_id);
    if (!fedTry.is_open()) {
        if (verboseLevel > 0) {
            cout << "... failed : ";
        }
        Debug(G, pdGendoc) << "exit Federation::Federation on exception CouldNotOpenFED" << endl;
        throw CouldNotOpenFED("RTIG have found but cannot open FED file");
    }
    else {
        if (verboseLevel > 0) {
            cout << "... opened." << endl;
        }
        fedTry.close();
    }

    bool is_a_fed = false;
    bool is_an_xml = false;

    // hope there is a . before fed or xml
    if (filename.at(filename.size() - 4) != '.') {
        Debug(G, pdGendoc) << "exit Federation::Federation on exception CouldNotOpenFED" << endl;
        throw CouldNotOpenFED(
            "Incorrect FED file name, cannot find extension (character '.' is missing [or not in reverse 4th place])");
    }

    string extension = filename.substr(filename.size() - 3);

    Debug(D, pdTrace) << "filename is: " << filename << " (extension is <" << extension << ">)" << endl;
    if (extension == "fed") {
        is_a_fed = true;
        Debug(D, pdTrace) << "Trying to use .fed file" << endl;
    }
    else if (extension == "xml") {
        is_an_xml = true;
        Debug(D, pdTrace) << "Trying to use .xml file" << endl;
    }
    else {
        Debug(G, pdGendoc) << "exit Federation::Federation on exception CouldNotOpenFED" << endl;
        throw CouldNotOpenFED("Incorrect FED file name : nor .fed nor .xml file");
    }

    ifstream fedFile(filename);

    if (fedFile.is_open()) {
        fedFile.close();
        if (is_a_fed) {
            // parse FED file and show the parse on stdout if verboseLevel>=2
            int err = fedparser::build(filename.c_str(), my_root_object.get(), (verboseLevel >= 2));
            if (err != 0) {
                Debug(G, pdGendoc) << "exit Federation::Federation on exception ErrorReadingFED" << endl;
                throw ErrorReadingFED("fed parser found error in FED file");
            }

            // Retrieve the FED file last modification time(for Audit)
            STAT_STRUCT StatBuffer;
#if defined(_WIN32) && _MSC_VER >= 1400
            char MTimeBuffer[26];
#else
            char* MTimeBuffer;
#endif

            if (STAT_FUNCTION(filename.c_str(), &StatBuffer) == 0) {
#if defined(_WIN32) && _MSC_VER >= 1400
                ctime_s(&MTimeBuffer[0], 26, &StatBuffer.st_mtime);
#else
                MTimeBuffer = ctime(&StatBuffer.st_mtime);
#endif
                MTimeBuffer[strlen(MTimeBuffer) - 1] = 0; // Remove trailing \n
                my_server->audit << "(Last modified " << MTimeBuffer << ")";
            }
            else
                my_server->audit << "(could not retrieve last modif time, errno " << errno << ").";
        }
        else if (is_an_xml) {
#ifdef HAVE_XML
            std::unique_ptr<XmlParser> parser;
            if (XmlParser::exists()) {
                switch (XmlParser::version(filename)) {
                case XmlParser::XML_IEEE1516_2000:
                case XmlParser::XML_LEGACY:
                    parser = make_unique<XmlParser2000>(my_root_object.get());
                    break;
                case XmlParser::XML_IEEE1516_2010:
                    parser = make_unique<XmlParser2010>(my_root_object.get());
                    break;
                }
                my_server->audit << ", XML File : " << filename;

                try {
                    parser->parse(filename);
                }
                catch (Exception* e) {
                    throw;
                }
            }
            else
#endif
            {
                cerr << "CERTI was Compiled without XML support" << endl;
                Debug(G, pdGendoc) << "exit Federation::Federation on exception CouldNotOpenFED" << endl;
                throw CouldNotOpenFED("Could not parse XML file. (CERTI Compiled without XML lib.)");
            }
        }
    }
}

bool Federation::saveXmlData()
{
#ifndef HAVE_XML
    return false;
#else
    xmlDocPtr doc = xmlNewDoc((const xmlChar*) "1.0");
    doc->children = xmlNewDocNode(doc, NULL, ROOT_NODE, NULL);

    xmlNodePtr federation;
    federation = xmlNewChild(doc->children, NULL, NODE_FEDERATION, NULL);

    xmlSetProp(federation, (const xmlChar*) "name", (const xmlChar*) my_name.c_str());

    char t[10];
    sprintf(t, "%u", my_handle.get());
    xmlSetProp(federation, (const xmlChar*) "handle", (const xmlChar*) t);

    xmlNodePtr federateXmlNode;

    //     for (HandleFederateMap::iterator i = _handleFederateMap.begin(); i != _handleFederateMap.end(); ++i) {
    for (const auto& kv : my_federates) {
        federateXmlNode = xmlNewChild(federation, NULL, NODE_FEDERATE, NULL);

        xmlSetProp(federateXmlNode, (const xmlChar*) "name", (const xmlChar*) kv.second->getName().c_str());

        sprintf(t, "%u", kv.second->getHandle());
        xmlSetProp(federateXmlNode, (const xmlChar*) "handle", (const xmlChar*) t);

        xmlSetProp(federateXmlNode,
                   (const xmlChar*) "constrained",
                   (const xmlChar*) ((kv.second->isConstrained()) ? "true" : "false"));
        xmlSetProp(federateXmlNode,
                   (const xmlChar*) "regulator",
                   (const xmlChar*) ((kv.second->isRegulator()) ? "true" : "false"));
    }

    xmlSetDocCompressMode(doc, 9);

    string filename = my_name + "_" + my_save_label + ".xcs";
    xmlSaveFile(filename.c_str(), doc);

    // TODO: tests

    return true;
#endif // HAVE_XML
}

bool Federation::restoreXmlData(string docFilename)
{
#ifndef HAVE_XML
    (void) docFilename;
    return false;
#else

    xmlDocPtr doc = xmlParseFile(docFilename.c_str());

    // Did libXML manage to parse the file ?
    if (doc == 0) {
        cerr << "XML restore file not parsed successfully" << endl;
        xmlFreeDoc(doc);
        return false;
    }
    xmlNodePtr cur;

    cur = xmlDocGetRootElement(doc);
    if (cur == 0) {
        cerr << "XML file is empty" << endl;
        xmlFreeDoc(doc);
        return false;
    }

    // Is this root element an ROOT_NODE ?
    if (xmlStrcmp(cur->name, ROOT_NODE)) {
        cerr << "Wrong XML file: not the expected root node" << endl;
        return false;
    }

    cur = cur->xmlChildrenNode;
    if (xmlStrcmp(cur->name, NODE_FEDERATION)) {
        cerr << "Wrong XML file structure" << endl;
        return false;
    }

    if (strcmp(my_name.c_str(), XmlParser::CleanXmlGetProp(cur, (const xmlChar*) "name")) != 0) {
        cerr << "Wrong federation name" << endl;
    }

    cur = cur->xmlChildrenNode;

    bool status = false;
    while (cur != NULL) {
        if ((!xmlStrcmp(cur->name, NODE_FEDERATE))) {
            //             for (HandleFederateMap::iterator i = _handleFederateMap.begin(); i != _handleFederateMap.end(); ++i) {
            for (const auto& kv : my_federates) {
                if (!strcmp(kv.second->getName().c_str(), XmlParser::CleanXmlGetProp(cur, (const xmlChar*) "name"))) {
                    // Set federate constrained status
                    status = !strcmp("true", XmlParser::CleanXmlGetProp(cur, (const xmlChar*) "constrained"));

                    try {
                        kv.second->setConstrained(status);
                    }
                    catch (RTIinternalError& e) {
                        Debug(D, pdDebug) << "Federate was already constrained, no issue" << endl;
                    }

                    // Set federate regulating status
                    status = !strcmp("true", XmlParser::CleanXmlGetProp(cur, (const xmlChar*) "regulator"));

                    try {
                        kv.second->setRegulator(status);
                    }
                    catch (RTIinternalError& e) {
                        Debug(D, pdDebug) << "Federate was already regulator, no issue" << endl;
                    }

                    try {
                        kv.second->setHandle(strtol(XmlParser::CleanXmlGetProp(cur, (const xmlChar*) "handle"), 0, 10));
                    }
                    catch (RTIinternalError& e) {
                        Debug(D, pdDebug) << "Federate handle was already set, no issue" << endl;
                    }
                    break;
                }
            }
        }
        cur = cur->next;
    }

    return status;
#endif // HAVE_XML
}

void Federation::broadcastAnyMessage(NetworkMessage* msg, FederateHandle except_federate, bool anonymous)
{
    Socket* socket = nullptr;

    // Broadcast the message 'msg' to all Federates in the Federation
    // except to Federate whose Handle is 'Except_Federate'.
    //     for (HandleFederateMap::iterator i = _handleFederateMap.begin(); i != _handleFederateMap.end(); ++i) {
    for (const auto& kv : my_federates) {
        if (anonymous || (kv.second->getHandle() != except_federate)) {
            try {
#ifdef HLA_USES_UDP
                socket = server->getSocketLink(kv.second->getHandle(), BEST_EFFORT);
#else
                socket = my_server->getSocketLink(kv.second->getHandle());
#endif
                msg->send(socket, my_nm_buffer);
            }
            catch (RTIinternalError& e) {
                Debug(D, pdExcept) << "Reference to a killed Federate while "
                                   << "broadcasting." << endl;
            }
            catch (NetworkError& e) {
                Debug(D, pdExcept) << "Network error while broadcasting, ignoring" << endl;
            }
        }
    }

    // BUG: If except = 0, could use Multicast.
}

void Federation::broadcastSomeMessage(NetworkMessage* msg,
                                      FederateHandle except_federate,
                                      const vector<FederateHandle>& fede_array,
                                      uint32_t nbfed)
{
    uint32_t ifed;
    Socket* socket = NULL;

    if (fede_array.size() != 0 || nbfed == 0) {
        // Broadcast the message 'msg' to some Federates (done in fede_array)
        // in the Federation
        // except to Federate whose Handle is 'Except_Federate'.
        //         for (HandleFederateMap::iterator i = _handleFederateMap.begin(); i != _handleFederateMap.end(); ++i) {
        for (const auto& kv : my_federates) {
            if (kv.second->getHandle() != except_federate) {
                ifed = 0;
                while (ifed < nbfed) {
                    if (kv.second->getHandle() == fede_array[ifed])
                    // Federate i has to be informed because into fede_array
                    {
                        try {
#ifdef HLA_USES_UDP
                            socket = server->getSocketLink(kv.second->getHandle(), BEST_EFFORT);
#else
                            socket = my_server->getSocketLink(kv.second->getHandle());
#endif
                            msg->send(socket, my_nm_buffer);
                        }
                        catch (RTIinternalError& e) {
                            Debug(D, pdExcept) << "Reference to a killed Federate while broadcasting" << endl;
                        }
                        catch (NetworkError& e) {
                            Debug(D, pdExcept) << "Network error while broadcasting, ignoring" << endl;
                        }
                    }
                    ifed++;
                }
            }
        }
    }

    // BUG: If except = 0, could use Multicast.
}

Responses Federation::respondToAll(std::unique_ptr<NetworkMessage> message, const FederateHandle except)
{
    Responses responses;

    std::vector<Socket*> sockets;
    for (const auto& pair : my_federates) {
        if (pair.first != except) {
#ifdef HLA_USES_UDP
            sockets.push_back(server->getSocketLink(pair.first, BEST_EFFORT));
#else
            sockets.push_back(my_server->getSocketLink(pair.first));
#endif
        }
    }

    responses.emplace_back(sockets, std::move(message));

    return responses;
}

Responses Federation::respondToSome(std::unique_ptr<NetworkMessage> message,
                                    const std::vector<FederateHandle> recipients)
{
    Responses responses;

    std::vector<Socket*> sockets;
    for (const auto& fed : recipients) {
#ifdef HLA_USES_UDP
        sockets.push_back(server->getSocketLink(fed, BEST_EFFORT));
#else
        sockets.push_back(my_server->getSocketLink(fed));
#endif
    }

    responses.emplace_back(sockets, std::move(message));

    return responses;
}

void Federation::enableMomIfAvailable()
{
    Debug(G, pdGendoc) << "enter Federation::enableMomIfAvailable" << endl;

    if (Mom::isAvailableInRootObjectAndCompliant(*my_root_object)) {
        try {
            my_mom = make_unique<Mom>(my_federate_handle_generator.provide(), *this, *my_root_object);

            my_server->registerMomFederateHandle(my_mom->getHandle());

            my_mom->publishObjects();
            my_mom->publishAndSubscribeInteractions();

            my_mom->registerFederation();

            std::cout << "MOM enabled with federate handle " << my_mom->getHandle() << endl;
        }
        catch (Exception& e) {
            my_mom.release();
            std::cout << "Error while setting up MOM: " << e.name() << " (" << e.reason() << ")" << std::endl;
        }
    }
    else {
        std::cout << "MOM disabled" << endl;
    }

    Debug(G, pdGendoc) << "exit  Federation::enableMomIfAvailable" << endl;
}

void Federation::setAutoProvide(const bool value)
{
    my_auto_provide = value;

    // TODO implement auto provide
    Debug(D, pdDebug) << "Auto Provide not yet implemented." << endl;

    if (my_mom) {
        my_mom->updateAutoProvide(value);
    }
}
}
} // namespace certi/rtig
