#ifndef ROSE_BINARYLOADER_H
#define ROSE_BINARYLOADER_H

/** Base class for loading a static or dynamic object.
 *
 *  The BinaryLoader class is the base class that defines the public interface and provides generic implementations for
 *  loading a static or dynamic object.  "Loading" means parsing, linking, mapping, and relocating, which are defined as
 *  follows:
 *
 *  <ul>
 *    <li>Parsing: reading the contents of a binary file and parsing the container format (ELF, PE, COFF, Dwarf, etc) to
 *        produce an abstract syntax tree (AST).  Although the container is parsed, the actual machine instructions are
 *        not (that is, disassembly is not part of loading).</li>
 *    <li>Linking: recursively parsing all shared object dependencies. This step does not include mapping memory into a
 *        process address space or fixing up relocation information.</li>
 *    <li>Mapping: choosing virtual addresses for parts of the binary file as if ROSE were creating a new OS process. For
 *        instance, mapping an ELF file will cause ROSE to choose virtual addresses for all the ELF Segments.  This has
 *        two side effects. First, the SgAsmGenericSection::get_mapped_actual_rva() method will now return the address that
 *        ROSE chose for the section rather than the same value as get_mapped_preferred_rva(), although they will often still
 *        be the same after loading. Second, the MemoryMap object returned by SgAsmInterpretation::get_map() will be updated
 *        to include a mapping from virtual addresses to the SgAsmGenericSection content.</li>
 *    <li>Relocation: applying relocation fixups to patch pointers and offsets in various parts of the virtual address
 *        space.</li>
 *  </ul>
 * 
 *  Some goals of this design are:
 *  <ul>
 *    <li>to load an entire binary into memory in a manner similar to how the operating system would do so.</li>
 *    <li>to generically handle both static and dynamic executables, treating static as a special case of dynamic.</li>
 *    <li>to handle a variety of binary container formats (ELF, PE, etc) and loaders (Linux and Windows).</li>
 *    <li>to be able to control the locations of shared object dependencies without affecting how ROSE itself is loaded.</li>
 *    <li>to be able to emulate linking of one operating system while ROSE is running in another operating system.</li>
 *    <li>to load an entire binary at once or one part at a time. This is useful when emulating ldopen().</li>
 *    <li>to be able to partially load an object. Useful when not all libraries are available.</li>
 *    <li>to allow a user to register a new loader subclass at runtime and have ROSE use it when appropriate.</li>
 *  </ul>
 *
 *  The general design is similar to the Disassembler class.  The BinaryLoader has class methods to register user-defined
 *  loaders with the library and each loader is able to answer whether it is capable of loading a particular kind of binary.
 *  ROSE (or any user of this class) can obtain a suitable loader for a particular SgAsmInterpretation, clone it (if desired),
 *  modify properties that control its behavior, and use it to load a binary.
 */
class BinaryLoader {
    /*========================================================================================================================
     * Exceptions
     *======================================================================================================================== */
public:
    /** Base class for exceptions thrown by loaders. */
    class Exception {
    public:
        Exception(const std::string &reason)
            : mesg(reason)
            {}
        void print(std::ostream&) const;
        friend std::ostream& operator<<(std::ostream&, const Exception&);
        std::string mesg;
    };

    /*========================================================================================================================
     * Constructors, etc.
     *======================================================================================================================== */
public:
    BinaryLoader()
        : debug(NULL), p_perform_dynamic_linking(false), p_perform_remap(true), p_perform_relocations(false)
        { init(); }

    BinaryLoader(const BinaryLoader &other)
        : debug(other.debug), p_perform_dynamic_linking(other.p_perform_dynamic_linking),
          p_perform_remap(other.p_perform_remap), p_perform_relocations(other.p_perform_relocations) {
        preloads = other.preloads;
        directories = other.directories;
    }

    virtual ~BinaryLoader(){}

private:
    /** Initialize the class. Register built-in loaders. */
    static void initclass();

    /*========================================================================================================================
     * Types
     *======================================================================================================================== */
public:
    enum Contribution {
        CONTRIBUTE_NONE,                /**< Section does not contribute to final mapping. */
        CONTRIBUTE_ADD,                 /**< Section is added to the mapping. */
        CONTRIBUTE_SUB                  /**< Section is subtracted from the mapping. */
    };

    /** The interface for deciding whether a section contributes to a mapping.
     *
     *  A Selector is used to decide whether a section should contribute to the mapping, and whether that contribution should
     *  be additive or subtractive.  Any section that contributes to a mapping will be passed through the align_values()
     *  method, which has an opportunity to veto the section's contribution. The Selector virtual class will be subclassed for
     *  various kinds of selections. */
    class Selector {
    public:
        virtual ~Selector() {}
        virtual Contribution contributes(SgAsmGenericSection*) = 0;
    };

    /*==========================================================================================================================
     * Registration and lookup methods
     *========================================================================================================================== */
public:
    /** Register a loader instance.  More specific loader instances should be registered after more general loaders since the
     *  lookup() method will inspect loaders in reverse order of their registration. */
    static void register_subclass(BinaryLoader*);

    /** Predicate determining the suitability of a loader for a specific file header.  If this loader is capable of loading
     *  the specified file header, then this predicate returns true, otherwise it returns false.  The implementation in
     *  BinaryLoader always returns true because BinaryLoader is able to generically load all types of files, albeit with
     *  limited functionality. Subclasses should certainly redefine this method so it returns true only for certain headers. */
    virtual bool can_load(SgAsmGenericHeader*) const {
        return true;
    }

    /** Finds a suitable loader. Looks through the list of registered loader instances (from most recently registered to
     *  earliest registered) and returns the first one whose can_load() predicate returns true.  Throws an exception if no
     *  suitable loader can be found. */
    static BinaryLoader *lookup(SgAsmGenericHeader*);
    
    /** Finds a suitable loader. Looks through the list of registered loader instances (from most recently registered to
     *  earliest registered) and returns the first one whose can_load() predicate returns true. This is done for each header
     *  contained in the interpretation and the loader for each header must match the other headers. An exception is thrown if
     *  no suitable loader can be found. */
    static BinaryLoader *lookup(SgAsmInterpretation*);

    /** Creates a new copy of a loader. The new copy has all the same settings as the original. Subclasses that define data
     *  methods should certainly provide an implementation of this method, although all they'll need to change is the data
     *  type for the 'new' operator. */
    virtual BinaryLoader *clone() const {
        return new BinaryLoader(*this);
    }

    /*========================================================================================================================
     * Accessors for properties.
     *======================================================================================================================== */
public:
    void set_perform_dynamic_linking(bool b) { p_perform_dynamic_linking = b; }
    bool get_perform_dynamic_linking() const { return p_perform_dynamic_linking; }

    void set_perform_remap(bool b) { p_perform_remap = b; }
    bool get_perform_remap() const { return p_perform_remap; }

    void set_perform_relocations(bool b) { p_perform_relocations = b; }
    bool get_perform_relocations() const { return p_perform_relocations; }

    void set_debug(FILE *f) { debug = f; }
    FILE *get_debug() const { return debug; }

    /*========================================================================================================================
     * Searching for shared objects
     *======================================================================================================================== */
public:
    /** Adds a library to the list of pre-loaded libraries. These libraries are linked into the AST in the order they were
     *  added to the preload list. The @p libname should be either the base name of a library, such as "libm.so" (in which
     *  case the search directories are consulted) or a path-qualified name like "/usr/lib/libm.so". */
    void add_preload(const std::string &libname) {
        preloads.push_back(libname);
    }

    /** Returns the list of libraries that will be pre-loaded. These are loaded before other libraries even if the preload
     *  libraries would otherwise not be loaded. */
    const std::vector<std::string>& get_preloads() const {
        return preloads;
    }

    /** Adds a directory to the list of directories searched for libraries.  This is similar to the LD_LIBRARY_PATH
     *  environment variable of the ld-linux.so dynamic loader (see the ld.so man page). ROSE searches for libraries in
     *  directories in the order that directories were added. */
    void add_directory(const std::string &dirname) {
        directories.push_back(dirname);
    }

    /** Returns the list of shared object search directories. */
    const std::vector<std::string>& get_directories() const {
        return directories;
    }

    /** Given the name of a shared object, return the fully qualified name where the library is located in the file system.
     *  Throws a BinaryLoader::Exception if the library cannot be found. */
    virtual std::string find_so_file(const std::string &libname) const;

    /*========================================================================================================================
     * The main interface.
     *======================================================================================================================== */
public:
    /** Class method to parse, map, link, and/or relocate all interpretations of the specified binary composite. This should
     *  only be called for an SgBinaryComposite object that has been created but for which no binary files have been parsed
     *  yet.  It's only called from sageSupport.C by SgBinaryComposite::buildAST().  A BinaryLoader::Exception is thrown if
     *  there's an error of some sort. */
    static void load(SgBinaryComposite* composite);

    /** Conditionally parse, map, link, and/or relocate the interpretation according to properties of this loader. If an error
     *  occurs, a BinaryLoader::Exception will be thrown.  The interpretation must be one that can be loaded by this loader as
     *  returned by this loader's can_load() method. */
    virtual void load(SgAsmInterpretation*);

    /** Links an interpretation by parsing all shared objects required by that interpretation.  In other words, all
     *  dependencies of the interpretation are parsed and added to the AST.  As mentioned in the BinaryLoader documentation,
     *  this process is referred to as "linking".
     *
     *  Recursively perform a breadth-first search of all headers, starting with the headers already in the interpretation.
     *  For each header, obtain a list of necessary shared objects (pruning away those that have already been processed) and
     *  parse the shared object, adding it to the AST and adding its appropriate headers to the interpretation.  Parsing of
     *  the shared objects is performed by calling createAsmAST().
     *
     *  This process is recursive in nature. A dynamically linked executable has a list of libraries on which it depends, and
     *  those libraries also often have dependencies.  The recursion is breadth-first because ELF specifies a particular order
     *  that symbols should be resolved. Order is not important for a PE binary since its shared object symbols are scoped to
     *  a library.
     *
     *  The list of dependencies for a particular header is obtained by the getDLLs() method, which is also responsible for
     *  not returning any shared object that we've already parsed.
     *
     *  Throws a BinaryLoader::Exception if any error occurs. */
    virtual void linkDependencies(SgAsmInterpretation *interp);
    
    /** Maps sections of the interpretation into the virtual address space.  Throws a BinaryLoader::Exception if an error
     *  occurs. Ignores any mapping that might already have been defined, recalculating a new mapping from scratch. */
    virtual void remapSections(SgAsmInterpretation *interp);

    /** Performs relocation fixups on the specified interpretation. This should be called after sections are mapped into
     *  memory by remapSections(). Throws a BinaryLoader::Exception if an error occurs. */
    virtual void fixupSections(SgAsmInterpretation *interp);

    /*========================================================================================================================
     * Mapping functions
     *======================================================================================================================== */
public:
    /** Creates a map containing all mappable sections in the file. If @p map is non-null then it will be modified in place
     *  and returned, otherwise a new map is created. */
    virtual MemoryMap *map_all_sections(MemoryMap *map, SgAsmGenericFile *file, bool allow_overmap=true) {
        return map_all_sections(map, file->get_sections(), allow_overmap);
    }

    /** Creates a map containing all mappable sections in a file header. If @p map is non-null then it will be modified in place
     *  and returned, otherwise a new map is created. */
    virtual MemoryMap *map_all_sections(MemoryMap *map, SgAsmGenericHeader *fhdr, bool allow_overmap=true) {
        return map_all_sections(map, fhdr->get_sections()->get_sections(), allow_overmap);
    }

    /** Creates a map containing the specified sections (if they are mapped). If @p map is non-null then it will be modified
     *  in place and returned, otherwise a new map is created. */
    virtual MemoryMap *map_all_sections(MemoryMap*, const SgAsmGenericSectionPtrList &sections, bool allow_overmap=true);



    /** Creates a map for all code-containing sections in the file. If @p map is non-null then it will be modified in place
     *  and returned, otherwise a new map is created. */
    virtual MemoryMap *map_code_sections(MemoryMap *map, SgAsmGenericFile *file, bool allow_overmap=true) {
        return map_code_sections(map, file->get_sections(), allow_overmap);
    }

    /** Creates a map for all code-containing sections reachable from the file header. If @p map is non-null then it will be
     *  modified in place and returned, otherwise a new map is created. */
    virtual MemoryMap *map_code_sections(MemoryMap *map, SgAsmGenericHeader *fhdr, bool allow_overmap=true) {
        return map_code_sections(map, fhdr->get_sections()->get_sections(), allow_overmap);
    }

    /** Creates a map for all code-containing sections from the specified list. If @p map is non-null then it will be
     *  modified in place and returned, otherwise a new map is created. */
    virtual MemoryMap *map_code_sections(MemoryMap*, const SgAsmGenericSectionPtrList &sections, bool allow_overmap=true);



    /** Creates a map for all executable sections in the file. If @p map is non-null then it will be
     *  modified in place and returned, otherwise a new map is created. */
    virtual MemoryMap *map_executable_sections(MemoryMap *map, SgAsmGenericFile *file, bool allow_overmap=true) {
        return map_executable_sections(map, file->get_sections(), allow_overmap);
    }

    /** Creates a map for all executable sections reachable from the file header. If @p map is non-null then it will be
     *  modified in place and returned, otherwise a new map is created. */
    virtual MemoryMap *map_executable_sections(MemoryMap *map, SgAsmGenericHeader *fhdr, bool allow_overmap=true) {
        return map_executable_sections(map, fhdr->get_sections()->get_sections(), allow_overmap);
    }

    /** Creates a map for all executable sections from the specified list. If @p map is non-null then it will be
     *  modified in place and returned, otherwise a new map is created. */
    virtual MemoryMap *map_executable_sections(MemoryMap*, const SgAsmGenericSectionPtrList &sections, bool allow_overmap=true);



    /** Creates a map for all writable sections in the file. If @p map is non-null then it will be
     *  modified in place and returned, otherwise a new map is created. */
    virtual MemoryMap *map_writable_sections(MemoryMap *map, SgAsmGenericFile *file, bool allow_overmap=true) {
        return map_writable_sections(map, file->get_sections(), allow_overmap);
    }

    /** Creates a map for all writable sections reachable from the file header. If @p map is non-null then it will be
     *  modified in place and returned, otherwise a new map is created. */
    virtual MemoryMap *map_writable_sections(MemoryMap *map, SgAsmGenericHeader *fhdr, bool allow_overmap=true) {
        return map_writable_sections(map, fhdr->get_sections()->get_sections(), allow_overmap);
    }

    /** Creates a map for all writable sections from the specified list. If @p map is non-null then it will be
     *  modified in place and returned, otherwise a new map is created. */
    virtual MemoryMap *map_writable_sections(MemoryMap*, const SgAsmGenericSectionPtrList &sections, bool allow_overmap=true);

    /** Creates a memory map containing sections that satisfy some constraint.
     *
     *  The sections are mapped in the order specified by order_sections(), contribute to the final mapping according to the
     *  specified Selector, and are aligned according to the rules in align_values().  If @p allow_overmap is set (the
     *  default) then any section that contributes to the map in an additive manner will first have its virtual address space
     *  removed from the map in order to prevent a MemoryMap::Inconsistent exception; otherwise a MemoryMap::Inconsistent
     *  exception will be thrown when a single virtual address would map to two different source bytes, such as two different
     *  offsets in a file.
     *
     *  If a memory map is supplied, then it will be modified in place and returned; otherwise a new map is allocated. */
    virtual MemoryMap *create_map(MemoryMap *map, const SgAsmGenericSectionPtrList&, Selector*, bool allow_overmap=true);


    

    /*========================================================================================================================
     * Supporting types and functions
     *======================================================================================================================== */
protected:
    /** Returns true if the specified file name is already linked into the AST. */
    virtual bool is_linked(SgBinaryComposite *composite, const std::string &filename);
    /** Returns true if the specified file name is already linked into the AST. */
    virtual bool is_linked(SgAsmInterpretation *interp, const std::string &filename);

    /** Parses a single binary file. The file may be an executable, core dump, or shared library.  The machine instructions in
     *  the file are not parsed--only the binary container is parsed.  The new SgAsmGenericFile is added to the supplied
     *  binary @p composite and a new interpretation is created if necessary.  Dwarf debugging information is also parsed and
     *  added to the AST if Dwarf support is enable and the information is present in the binary container. */
    static SgAsmGenericFile *createAsmAST(SgBinaryComposite *composite, std::string filePath);

    /** Finds shared object dependencies of a single binary header.  Returns a list of dependencies, which are usually library
     *  names rather than actual files.  The library names can be turned into file names by calling find_so_file().  Only one
     *  header is inspected (i.e., this function is not recursive) and no attempt is made to remove names from the return
     *  value that have already been parsed into the AST. */
    virtual std::vector<std::string> dependencies(SgAsmGenericHeader*);


    /** Selects those sections which should be layed out by the Loader and inserts them into the @p allSections argument.  The
     *  default implementation (in this base class) is to add all sections to the list. Subclasses will likely restrict this
     *  to a subset of sections. */
    virtual void addSectionsForRemap(SgAsmGenericHeader* header, SgAsmGenericSectionPtrList &allSections);

#if 0
    /** Helper function to get raw dll list from a file. */
    virtual Rose_STL_Container<std::string> getDLLs(SgAsmGenericHeader* header,
                                                    const Rose_STL_Container<std::string> &dllFilesAlreadyLoaded);
#endif

    /** Find all headers in @p candidateHeaders that are similar to @p matchHeader. This is used to determine whether two
     *  headers can be placed in the same SgAsmInterpretation. We make this determination by looking at whether the
     *  Disassembler for each header is the same.  In other words, an x86_64 header will not be similar to an i386 header even
     *  though they are both ELF headers and both x86 architectures. */
    static SgAsmGenericHeaderPtrList findSimilarHeaders(SgAsmGenericHeader *matchHeader,
                                                        SgAsmGenericHeaderPtrList &candidateHeaders);

    /** Determines whether two headers are similar enough to be in the same interpretation.  Two headers are similar if
     *  disassembly would use the same Disassembler for both.  See findSimilarHeaders(). */
    static bool isHeaderSimilar(SgAsmGenericHeader*, SgAsmGenericHeader*);

    /** Computes memory mapping addresses for a section.
     *
     *  Operating systems generally have some alignment constraints for how a file is mapped to virtual memory, and these
     *  correspond to the page size. For example, see the man page for Unix mmap().  There are three parts to the information
     *  returned:
     *
     *  First, we need to know the entire range of file offsets that are mapped. On Unix systems only entire pages of a file
     *  can be mapped. If the section begins with a partial page then we align the starting offset downward. Likewise if the
     *  section ends with a partial page we extend the range.  Both of these adjustments can increase the number of file bytes
     *  that are mapped.  The starting offset and file size are returned through the @p offset and @p file_size arguments.
     *
     *  Second, we need to know the entire range of virtual addresses that are affected.  Again, at least on Unix systems,
     *  only entire pages of memory can be mapped. We therefore make the same adjustments to memory as we did to the file,
     *  aligning the starting virtual address downward and the ending address upward. Both of these adjustments can affect the
     *  size of the virtual address region affected.  The starting address and memory size are returned through the @p va and
     *  @p mem_size arguments.  When the returned memory size is larger than the returned file size then the mapping will
     *  include an anonymous region (memory that is initialized to zero rather than file contents).
     *
     *  Finally, we need to indicate where the first byte of the section is located in memory. If we needed to make
     *  adjustments to the first affected virtual address then the start-of-section will not be the same as the first affected
     *  virtual address.  The start-of-section virtual address is the return value of this function.
     *
     *  Additionally, this function can veto a section mapping by returning a zero memory size.  This can happen when the
     *  Selector selects a section indiscriminantly (as with map_all_sections()) but there is not enough information to
     *  determine where the section should be mapped.  On the other hand, it can also define a mapping for a section that
     *  doesn't have any mapping attributes (for instance, the ELF object file loader, LoaderELFObj, maps function text
     *  sections that are marked as "not mapped" in the ELF container by choosing free regions of virtual memory using the @p
     *  current argument that contains the most up-to-date mapping). */
    virtual rose_addr_t align_values(SgAsmGenericSection*, Contribution,
                                     rose_addr_t *va,     rose_addr_t *mem_size,
                                     rose_addr_t *offset, rose_addr_t *file_size,
                                     const MemoryMap *current);

    /** Sort sections for mapping.
     *
     *  Given a list of sections, sort the sections according to the order they should contribute to the mapping.  For
     *  instance, ELF Segments should contribute to the mapping before ELF Sections.
     *
     *  This method can also be used as a first-line for excluding sections that meet certain criteria. It is called by
     *  create_map() before sections are passed through the Selector object.  The default implementation excludes any section
     *  that was synthesized by the binary parser, keeping only those that were created due to the parser encountering a
     *  description of the section in some kind of table. */
    virtual SgAsmGenericSectionPtrList order_sections(const SgAsmGenericSectionPtrList&);



    /*========================================================================================================================
     * Private stuff
     *======================================================================================================================== */
private: 
    void init();                                        /**< Further initializations in a *.C file. */

    static std::vector<BinaryLoader*> loaders;          /**< List of loader subclasses. */
    std::vector<std::string> preloads;                  /**< Libraries that should be pre-loaded. */
    std::vector<std::string> directories;               /**< Directories to search for libraries with relative names. */
    FILE *debug;                                        /**< Stream where diagnostics are sent; null to disable them. */

    bool p_perform_dynamic_linking;
    bool p_perform_remap;
    bool p_perform_relocations;
};

#endif /* ROSE_BINARYLOADER_H */
