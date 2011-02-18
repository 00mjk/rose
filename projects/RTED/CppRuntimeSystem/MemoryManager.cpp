#include <sstream>
#include <iostream>

#include "MemoryManager.h"

#include "CppRuntimeSystem.h"

// -----------------------    MemoryType  --------------------------------------

MemoryType::MemoryType(Location _addr, size_t _size, AllocKind wherehow, const SourcePosition & _pos)
: startAddress(_addr), size(_size), initialized(_size, false), origin(wherehow), allocPos(_pos)
{
  // not memory has been initialized by default
  // \why pp?
	RuntimeSystem * rs = RuntimeSystem::instance();

  std::stringstream msg;

  msg << "	***** Allocate Memory ::: " << _addr
      << "  size:" << _size
      << "  pos1:" + _pos.getLineInOrigFile();

  rs->printMessage(msg.str());
}

void MemoryType::resize( size_t new_size ) {
    assert( new_size >= size );
    size = new_size;
}

bool MemoryType::containsAddress(Location queryAddress)
{
    return (  startAddress <= queryAddress
           && queryAddress < startAddress + size
           );
}

bool MemoryType::containsMemArea(Location queryAddr, size_t querySize)
{
    return (  containsAddress(queryAddr)
           && containsAddress(queryAddr + (querySize - 1))
           );
}

bool MemoryType::overlapsMemArea(Location queryAddr, size_t querySize)
{
    return (  containsAddress(queryAddr)
           || containsAddress(queryAddr + (querySize-1))
           );
}

bool MemoryType::operator< (const MemoryType& other) const
{
    return (startAddress < other.startAddress);
}


bool MemoryType::isInitialized(size_t offsetFrom, size_t offsetTo) const
{
    assert(offsetFrom < offsetTo);
    assert(offsetTo <= size);

    for (size_t i= offsetFrom; i < offsetTo; i++)
        if (! initialized[i])
            return false;

    return true;
}

void MemoryType::initialize(size_t offsetFrom, size_t offsetTo)
{
    assert(offsetFrom < offsetTo);
    assert(offsetTo <= size);

    for(size_t i=offsetFrom; i<offsetTo; i++)
        initialized[i]=true;
}



void MemoryType::registerMemType(size_t offset, RsType * type)
{
    RuntimeSystem * rs = RuntimeSystem::instance();
    rs->printMessage("   ++ registerMemType at offset: " + ToString(offset) + "  type: "+type->getName());

    bool types_merged = checkAndMergeMemType(offset, type );
    rs->printMessage("   ++ types_merged: " + ToString(types_merged));

    if( !types_merged ) {
        // type wasn't merged, some previously unknown portion of this
        // MemoryType has been registered with type
        typeInfo.insert(TypeInfoMap::value_type(offset, type));

        // if we have knowledge about the type in memory, we also need to update the
        // information about "dereferentiable" memory regions i.e. pointer
        rs->getPointerManager()->createPointer(startAddress + offset, type);
    }

    rs->printMessage("   ++ registerMemType done.");
}

void MemoryType::forceRegisterMemType(size_t offset, RsType* type ) {
    TiIterPair type_range = getOverlappingTypeInfos(offset, offset + type->getByteSize());

    typeInfo.erase(type_range.first, type_range.second);
    typeInfo.insert(TypeInfoMap::value_type(offset, type));
}

bool MemoryType::checkAndMergeMemType(size_t offset, RsType * type)
{
  // std::cout << ">> checkAndMergeMemType " << typeInfo.size() << std::endl;

    if (typeInfo.size() ==0) // no types registered yet
        return false;

    RuntimeSystem * rs = RuntimeSystem::instance();

    size_t newTiStart = offset;
    size_t newTiEnd = offset + type->getByteSize();

    rs->printMessage( "   ++ checkAndMergeMemType newTiStart: "
                    + ToString(newTiStart)
				            + "  newTiEnd: "
                    + ToString(newTiEnd)
                    );

    std::stringstream ss;
    ss << "Tried to access the same memory region with different types" << std::endl;
    ss << "When trying to access (" << newTiStart << "," << newTiEnd << ") "
       << "as type " << type->getName() << std::endl;
    ss << "Memory Region Info: " << std::endl;
    ss << *this << std::endl;

    RuntimeViolation vio(RuntimeViolation::INVALID_TYPE_ACCESS, ss.str());

    // Get a range of entries which overlap with [newTiStart,newTiEnd)
    TiIterPair res = getOverlappingTypeInfos(newTiStart, newTiEnd);
    TiIter     itLower = res.first;
    TiIter     itUpper = res.second;

    // Case 0: There are types registered, but not ϵ [newTiStart, newTiEnd)
    if (itLower == itUpper)
      return false;

    //  Case 1: New entry is overlapped by one old entry,
    //  i.e. [itLower,itUpper) contains only one entry
    TiIter     incrementedLower = itLower;
    ++incrementedLower;

    if (incrementedLower == itUpper)
    {
        rs->printMessage("     ++ incrementedLower == itUpper ");

        size_t   oldTiStart = itLower->first;
        size_t   oldTiEnd = oldTiStart + itLower->second->getByteSize();
        RsType*  oldType = itLower->second;

        if (oldTiStart <= newTiStart && newTiEnd <= oldTiEnd)
        {
            rs->printMessage("    oldTiStart <= newTiStart && oldTiEnd >= newTiEnd ");
            //Check if new entry is consistent with old one

            bool new_type_ok = oldType -> checkSubtypeRecursive( newTiStart - oldTiStart, type );

            rs->printMessage("      new_type_ok : " + ToString(new_type_ok));
            if( !new_type_ok )
            {
                vio.descStream() << "Previously registered Type completely overlaps new Type in an inconsistent way:"
                                 << std::endl
                                 << "Containing Type " << oldType->getName()
                                 << " (" << oldTiStart << "," << oldTiEnd << ")" << std::endl;

                rs->violationHandler(vio);
                return false;
            }
            else
            {
                // successful, no need to add because the type is already contained in bigger one
                // effectively type has already been “merged”.
                return true;
            }
        }
    }

    // Case 2: Iterate over overlapping old types and check consistency
    for(TypeInfoMap::iterator i = itLower; i != itUpper; ++i )
    {
        RsType*  curType = i->second;
        size_t   curStart = i->first;
        size_t   curEnd = curStart + curType->getByteSize();

        if ((curStart < newTiStart) || (newTiEnd < curEnd))
        {
            vio.descStream() << "Previously registered Type overlaps new Type in an inconsistent way:"
                             << std::endl
                             << "Overlapping Type " << curType->getName()
                             << " (" << curStart << "," << curEnd << ")"
                             << std::endl;
            rs->violationHandler(vio);
            return false;
        }

        RsType * sub =  type->getSubtypeRecursive(curStart - newTiStart,curType->getByteSize());
        if (sub != curType)
        {
            vio.descStream() << "Newly registered Type completely overlaps a previous registered Type in an inconsistent way:"
                             << std::endl
                             << "Overlapping Type " << curType->getName()
                             << " (" << curStart << "," << curEnd << ")" <<std::endl;

            rs->violationHandler(vio);
            return false;
        }
    }
    //All consistent - old typeInfos are replaced by big new typeInfo (merging)
    typeInfo.erase(itLower, itUpper);
    typeInfo.insert(TypeInfoMap::value_type(offset, type));

    // if we have knowledge about the type in memory, we also need to update the
    // information about "dereferentiable" memory regions i.e. pointer
    rs->getPointerManager()->createPointer(startAddress + offset, type);

    return true;
}




RsType* MemoryType::getTypeAt(size_t offset, size_t size)
{
    TiIterPair type_infos = getOverlappingTypeInfos(offset, offset + size);
    TiIter     first_addr_type = type_infos.first;
    TiIter     end = type_infos.second;

    if( first_addr_type == end )
        // We know nothing about the types at offset..+size
        return &RsType::UnknownType;

    RsType* res = NULL;
    TiIter  second_addr_type = first_addr_type;

    ++second_addr_type;
    if (  second_addr_type == end
       && offset >= first_addr_type->first // \pp what to do with negative offsets (see below)?
       )
    {
      // We know only something about one type in this range
      RsType* first_type = first_addr_type->second;

      res = first_type->getSubtypeRecursive( offset - first_addr_type->first,  // \pp negative?
                                             size,
                                             true
                                           );
    }

    return res;
}

RsType* MemoryType::computeCompoundTypeAt(size_t offset, size_t size)
{
    TiIterPair      type_infos = getOverlappingTypeInfos( offset, offset + size );
    TiIter          first_addr_type = type_infos.first;
    TiIter          end = type_infos.second;

    // We don't know the complete type of the range, but we do know something.
    // Construct a compound type with the information we have.
    RsCompoundType* computed_type = new RsCompoundType( "Partially Known Type", size );

    for( TiIter i = first_addr_type; i != end; ++i)
    {
        assert(i->first >= offset);

        size_t  subtype_offset = i->first - offset;
        RsType* type = i -> second;

        computed_type->addMember( "", type, subtype_offset );
    }

    return computed_type;
}


MemoryType::TiIter MemoryType::adjustPosOnOverlap(TiIter iter, size_t from)
{
  if (iter == typeInfo.begin()) return iter;

  TiIter prev = iter;

  --prev;
  return prev->first + prev->second->getByteSize() <= from ? iter : prev;
}

MemoryType::TiIterPair MemoryType::getOverlappingTypeInfos(size_t from, size_t to)
{
    // This function assumes that the typeInfo ranges do not overlap

    // the same for the beginning, but there we need the previous map entry
    TiIter lb = typeInfo.lower_bound(from);

    // end is the iterator which points to typeInfo with offset >= to
    TiIter ub = typeInfo.lower_bound(to);

    lb = adjustPosOnOverlap(lb, from);

    return TiIterPair(lb, ub);
}



std::string MemoryType::getInitString() const
{
    std::string init = "Part. initialized";
    bool allInit  = true;
    bool noneInit = false;

    for(std::vector<bool>::const_iterator it = initialized.begin(); it != initialized.end(); ++it)
    {
      noneInit = noneInit || *it;
      allInit = allInit && *it;
    }

    if(allInit)
        init = "Initialized      ";
    else if (noneInit)
        init = "Not initialized  ";

    return init;
}

void MemoryType::print() const
{
    print( std::cerr );
}

// extra print function because operator<< cannot be member-> no access to privates
void MemoryType::print(std::ostream & os) const
{
    os << "0x" << startAddress;
    os << " Size " << std::dec << size;
    os <<  "\t" << getInitString() << "\tAllocated at " << allocPos;
    os << std::endl;

    PointerManager * pm = RuntimeSystem::instance()->getPointerManager();
    typedef PointerManager::TargetToPointerMapIter Iter;
    Iter it =  pm->targetRegionIterBegin(startAddress);
    Iter end = pm->targetRegionIterEnd(startAddress+size);
    if(it != end)
    {
        os << "\tPointer to this chunk: ";
        for(; it != end; ++it)
        {
            VariablesType * vt =  it->second->getVariable();
            if(vt)
                os << "\t" << vt->getName() << " ";
            else
                os << "\t" << "Addr:0x" << it->second->getSourceAddress() << " ";
        }
        os <<std::endl;
    }

    if(typeInfo.size() > 0)
    {
        os << "\t" << "Type Info" <<std::endl;
        for(TypeInfoMap::const_iterator i = typeInfo.begin(); i != typeInfo.end(); ++i)
            os << "\t" << i->first << "\t" << i->second->getName() <<std::endl;
    }
}

std::ostream& operator<< (std::ostream &os, const MemoryType & m)
{
    m.print(os);
    return os;
}




// -----------------------    MemoryManager  --------------------------------------

MemoryManager::MemoryManager()
{
}

MemoryManager::~MemoryManager()
{
  checkForNonFreedMem();
}


MemoryType* MemoryManager::findPossibleMemMatch(Location addr)
{
  if (mem.size()==0)
    return NULL;

  // search the memory chunk, we get the chunk which has equal or greater startAdress
  MemoryTypeSet::iterator it = mem.lower_bound(addr);

  // case 0: we are at the end and map.size>0, so return last element
  if (it == mem.end())
  {
    --it;
    return &(it->second);
  }

  //case 1: addr is startAdress of a chunk, exact match
  if (it->first == addr)
  {
    return &(it->second);
  }

  //case 2: addr is greater than a chunk startadress
  //        try to get next smaller one
  assert(addr < it->first);

  if (it == mem.begin())
    return NULL;

  --it;
  return &(it->second);
}

MemoryType* MemoryManager::findContainingMem(Location addr, size_t size)
{
    MemoryType * res = findPossibleMemMatch(addr);

    return (res && res->containsMemArea(addr,size)) ?  res  : NULL;
}

bool MemoryManager::existOverlappingMem(Location addr, size_t size)
{
    MemoryType * res = findPossibleMemMatch(addr);

    return res && res->overlapsMemArea(addr, size);
}



MemoryType* MemoryManager::allocateMemory(Location adrObj, size_t szObj, MemoryType::AllocKind kind, const SourcePosition& pos)
{
    if (szObj == 0)
    {
        RuntimeSystem::instance()->violationHandler(RuntimeViolation::EMPTY_ALLOCATION,"Tried to call malloc/new with size 0\n");
        return NULL;
    }

    if (existOverlappingMem(adrObj, szObj))
    {
        // the start address of new chunk lies in already allocated area
        RuntimeSystem::instance()->violationHandler(RuntimeViolation::DOUBLE_ALLOCATION);
        return NULL;
    }

    MemoryType tmp(adrObj, szObj, kind, pos);

    return &mem.insert(MemoryTypeSet::value_type(adrObj, tmp)).first->second;
}

static
std::string allocDisplayName(AllocKind ak)
{
  const char* res = NULL;

  switch (ak)
  {
    case akStack          : res = "Stack"; break;
    case akCHeap          : res = "malloc/calloc"; break;

    /* C++ */
    case akCxxNew         : res = "operator new"; break;
    case akCxxArrayNew    : res = "operator new[]"; break;

    /* UPC */
    case akUpcAlloc       : res = "upc_alloc/upc_local_alloc"; break;
    case akUpcGlobalAlloc : res = "upc_global_alloc"; break;
    case akUpcAllAlloc    : res = "upc_all_alloc"; break;
    default               : res = "unknown allocation method";
  }

  assert(res != NULL);
  return res;
}

static
std::string freeDisplayName(AllocKind ak)
{
  const char* res = NULL;

  switch (ak)
  {
    case akStack          : res = "stack (rted bug?) free"; break;
    case akCHeap          : res = "free"; break;

    /* C++ */
    case akCxxNew         : res = "operator delete"; break;
    case akCxxArrayNew    : res = "operator delete[]"; break;

    /* UPC */
    case akUpcSharedHeap  : res = "upc_free"; break;
    default               : res = "unknown deallocation method";
  }

  assert(res != NULL);
  return res;
}

static
std::string
alloc_free_mismatch(MemoryType& m, AllocKind howAlloced, AllocKind howFreed)
{
  std::stringstream desc;

  if (howAlloced == akStack)
  {
    desc << "Stack memory was explicitly freed (0x" << m << ")" << std::endl;
  }
  else
  {
    assert((howAlloced > akStack) && (howFreed > akStack));

    desc << "Memory allocated with '" << allocDisplayName(howAlloced) << "' "
         << "but deallocated with '"  << freeDisplayName(howFreed)    << "' "
         << "0x" << m
         << std::endl;
  }

  return desc.str();
}


void MemoryManager::freeMemory(Location addr, MemoryType::AllocKind freekind)
{
    RuntimeSystem* rs = RuntimeSystem::instance();
    MemoryType*    m = findContainingMem(addr, 1);

    // free on unknown address
    if (m == NULL)
    {
        std::stringstream desc;
        desc << freeDisplayName(freekind);
        desc << " was called with unknown address " << addr <<std::endl;
        desc << " Allocated Memory Regions:" << std::endl;
        print(desc);
        rs->violationHandler(RuntimeViolation::INVALID_FREE, desc.str());
        return;
    }

    // free inside an allocated memory block
    if (m->getAddress() != addr)
    {
        std::stringstream desc;
        desc << freeDisplayName(freekind);
        desc << " was called with an address inside of an allocated block (Offset:"
             << " 0x" << std::hex << (addr - m->getAddress()) <<")" <<std::endl;
        desc << "Allocated Block: " << *m <<std::endl;

        rs->violationHandler(RuntimeViolation::INVALID_FREE, desc.str());
        return;
    }

    // free stack memory explicitly (i.e. not via exitScope)
    MemoryType::AllocKind allockind = m->howCreated();

    // allockind can be different from freekind
    //   e.g., upc_free subsumes all upc allocations
    if ((allockind & freekind) != freekind)
    {
      std::string msg = alloc_free_mismatch(*m, allockind, freekind);
      rs->violationHandler(RuntimeViolation::INVALID_FREE, msg);
      return;
    }

    PointerManager* pm = rs->getPointerManager();
    Location        from = m->getAddress();
    Location        to = from + m->getSize();

    pm->deletePointerInRegion( from, to, true );
    pm->invalidatePointerToRegion( from, to );

    // successful free, erase allocation info from map
    mem.erase(m->getAddress());
}



MemoryType* MemoryManager::checkAccess(Location addr, size_t size, RsType * t, RuntimeViolation::Type vioType)
{
    RuntimeSystem*    rs = RuntimeSystem::instance();
    MemoryType *const res = findContainingMem(addr,size);

    if(!res)
    {
        std::stringstream desc;
        desc << "Trying to access non-allocated MemoryRegion "
             << "(Address " << addr
             << " of size " << std::dec << size << ")"
             << std::endl;

        const MemoryType * possMatch = findPossibleMemMatch(addr);
        if(possMatch)
            desc << "Did you try to access this region:" <<std::endl << *possMatch <<std::endl;

        rs->violationHandler(vioType,desc.str());
    }

    if (t)
    {
      std::stringstream msg;

      msg << "   ++ found memory addr : " << addr << " size:" << ToString(size);
      rs->printMessage(msg.str());

    	res->registerMemType(addr - res->getAddress(), t);
    }

    return res;
}




void MemoryManager::checkRead(Location addr, size_t size, RsType * t)
{
    // \pp \note t seems to be always NULL

    MemoryType    * mt = checkAccess(addr,size,t,RuntimeViolation::INVALID_READ);
    assert(mt != NULL && mt->getAddress() <= addr);

    const int       from = addr - mt->getAddress();

    if(! mt->isInitialized(from, from +size) )
    {
      RuntimeSystem * rs = RuntimeSystem::instance();

      std::stringstream desc;
      desc << "Trying to read from uninitialized MemoryRegion "
           << "(Address " << "0x" << addr
           << " of size " << std::dec << size << ")"
           << std::endl;

      rs->violationHandler(RuntimeViolation::INVALID_READ,desc.str());
    }
}



void MemoryManager::checkWrite(Location addr, size_t size, RsType * t)
{
    RuntimeSystem *   rs = RuntimeSystem::instance();
    std::stringstream msg;

    msg << "   ++ checkWrite : " << addr << " size:" << size;
    rs->printMessage(msg.str());

    MemoryType *    mt = checkAccess(addr,size,t,RuntimeViolation::INVALID_WRITE);
    assert(mt && mt->getAddress() <= addr);

    const int       from = addr - mt->getAddress();

    mt->initialize(from,from + size);
    rs->printMessage("   ++ checkWrite done.");
}

bool MemoryManager::isInitialized(Location addr, size_t size)
{
    MemoryType* mt = checkAccess(addr,size,NULL,RuntimeViolation::INVALID_READ);
    const int   offset = addr - mt->getAddress();

    return mt->isInitialized(offset,offset+size);
}


bool MemoryManager::checkIfSameChunk(Location addr1, Location addr2, RsType * type)
{
	return checkIfSameChunk( addr1, addr2, type->getByteSize() );
}

bool MemoryManager::checkIfSameChunk( Location addr1,
		                                  Location addr2,
		                                  size_t typeSize,
		                                  RuntimeViolation::Type violation
                                    )
{
  RuntimeSystem *        rs = RuntimeSystem::instance();
  RuntimeViolation::Type access_violation = violation;
  if (access_violation != RuntimeViolation::NONE) access_violation = RuntimeViolation::INVALID_READ;

  MemoryType *           mem1 = checkAccess(addr1, typeSize, NULL, access_violation);

  // check that addr1 and addr2 point to the same base location
  {
    MemoryType *         mem2 = checkAccess(addr2, typeSize, NULL, access_violation);

    if (violation == RuntimeViolation::NONE && !(mem1 && mem2))
    {
      return false;
    }

    assert(mem1 && mem2);
    if (mem1 != mem2)
    {
        std::stringstream ss;
        ss << "Pointer changed allocation block from "
           << addr1 << " to " << addr2
           << std::endl;

        rs->violationHandler( violation, ss.str() );
        return false;
    }
  }

  const Location        memaddr = mem1->getAddress();
  int                   off1 = addr1 - memaddr;
  int                   off2 = addr2 - memaddr;
  RsType*               type1 = mem1 -> getTypeAt( off1, typeSize );
  RsType*               type2 = mem1 -> getTypeAt( off2, typeSize );
  std::auto_ptr<RsType> guard1(NULL);
  std::auto_ptr<RsType> guard2(NULL);

  if( !type1 ) {
      type1 = mem1 -> computeCompoundTypeAt( off1, typeSize );
      guard1.reset(type1);
  }

  if( !type2 ) {
      type2 = mem1 -> computeCompoundTypeAt( off2, typeSize );
      guard2.reset(type2);
  }

  assert( type1 );
  assert( type2 );

  bool failed = !type1->isConsistentWith( *type2 );

  if (!failed)
  {
    RsArrayType* array = dynamic_cast< RsArrayType* >( type1 );

    if (  array
       && !(  off1 + array -> getByteSize() >= off2 + typeSize  // the array element might be after the array [ N ]...
           && off2 >= off1 // ... or before it [ -1 ]
           )
       )
    {
      // out of bounds error (e.g. int[2][3], ref [0][3])
      failed = true;
    }
  }

  if (failed)
    failNotSameChunk( type1, type2, off1, off2, mem1, mem1, violation );

	return !failed;
}

void MemoryManager::failNotSameChunk(
        RsType* type1,
        RsType* type2,
        size_t off1,
        size_t off2,
        MemoryType* mem1,
        MemoryType* mem2,
		RuntimeViolation::Type violation) {

    RuntimeSystem * rs = RuntimeSystem::instance();

    std::stringstream ss;
    ss << "A pointer changed the memory area (array or variable) which it points to (may be an error)" << std::endl <<std::endl;

    ss << "Region1:  " <<  *type1 << " at offset " << off1 << " in this Mem-Region:" << std::endl
                          << *mem1 <<std::endl;

    ss << "Region2: " << *type2 << " at offset " << off2 << " in this Mem-Region:" << std::endl
                          << *mem2 <<std::endl;

    rs->violationHandler( violation ,ss.str() );
}






void MemoryManager::checkForNonFreedMem() const
{
    std::stringstream desc;
    if(mem.size() !=0)
    {
        desc << "Program terminated and the following allocations were not freed:" <<std::endl;
        print(desc);

        RuntimeSystem::instance()->violationHandler(RuntimeViolation::MEMORY_LEAK,desc.str());
    }
}


MemoryType * MemoryManager::getMemoryType(Location addr)
{
    MemoryType * possibleMatch = findPossibleMemMatch(addr);

    return(
        possibleMatch != NULL
        && possibleMatch->getAddress() == addr
            ? possibleMatch
            : NULL
    );
}


void MemoryManager::print(std::ostream& os) const
{
    if (mem.size() ==0 )
    {
        os << "Memory Manager: No allocations" << std::endl;
        return;
    }

    os << std::endl;
    os << "------------------------------- Memory Manager Status -----------------------------" << std::endl << std::endl;
    for (MemoryTypeSet::const_iterator i = mem.begin(); i != mem.end(); ++i)
        os << i->second << std::endl;

    os << "-----------------------------------------------------------------------------------" << std::endl;
    os << std::endl;
}

std::ostream& operator<< (std::ostream &os, const MemoryManager& m)
{
    m.print(os);
    return os;
}
