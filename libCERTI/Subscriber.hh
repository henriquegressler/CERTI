// ----------------------------------------------------------------------------
// CERTI - HLA RunTime Infrastructure
// Copyright (C) 2002, 2003  ONERA
//
// This file is part of CERTI-libCERTI
//
// CERTI-libCERTI is free software ; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation ; either version 2 of
// the License, or (at your option) any later version.
//
// CERTI-libCERTI is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY ; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this program ; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
// USA
//
// $Id: Subscriber.hh,v 3.9 2005/03/15 14:34:51 breholee Exp $
// ----------------------------------------------------------------------------

#ifndef _CERTI_SUBSCRIBER_HH
#define _CERTI_SUBSCRIBER_HH

#include "certi.hh"
#include "RegionImp.hh"

namespace certi {

class RegionImp ;

class Subscriber
{
public:
    Subscriber(FederateHandle);
    Subscriber(FederateHandle, const RegionImp *);
    
    FederateHandle getHandle() const ;
    const RegionImp *getRegion() const ;
    bool match(FederateHandle, const RegionImp *) const ;
    bool match(const RegionImp *) const ;

protected:
    FederateHandle handle ; //!< The ID of the Subscriber.
    const RegionImp *region ; //!< the subscription region
};

}

#endif // _CERTI_SUBSCRIBER_HH

// $Id: Subscriber.hh,v 3.9 2005/03/15 14:34:51 breholee Exp $
