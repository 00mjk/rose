-- Schema for binary clone detection output
-- SQL contained herein should be written portably for either SQLite3 or PostgreSQL

-- Clean up. Tables need to be dropped in the opposite order they're created.
drop table if exists semantic_clusters;         -- The default name for the clusters table; not created in this SQL file
drop table if exists semantic_clusters_tpl;
drop table if exists semantic_funcsim;
drop table if exists semantic_fio_trace;
drop table if exists semantic_fio_events;
drop table if exists semantic_fio;
drop table if exists semantic_sources;
drop table if exists semantic_instructions;
drop table if exists semantic_cg;
drop table if exists semantic_binaries;
drop table if exists semantic_functions;
drop table if exists semantic_specfiles;
drop table if exists semantic_files;
drop table if exists semantic_faults;
drop table if exists semantic_outputvalues;
drop table if exists semantic_inputvalues;
drop table if exists semantic_input_queues;
drop table if exists semantic_history;

-- A history of the commands that were run to produce this database, excluding SQL run by the user.
create table semantic_history (
       hashkey bigint unique primary key,       -- nonsequential ID number to identify the command
       begin_time bigint,                       -- Approx time that command started (Unix time, seconds since epoch)
       end_time bigint,                         -- Approx time command ended, or zero if command terminated with failure
       notation text,                           -- Notation describing what action was performed
       command text                             -- Command-line that was issued
);

-- Table of input queue names.
create table semantic_input_queues (
       id integer primary key,			-- Queue ID or a special value (see InputGroupName)
       name varchar(16),  			-- Short name
       description text				-- Single-line, full description
);

-- This table encodes the set of all input groups.  Each input group consists of an input queue which contains zero or
-- more values.  This table uses special values for queue_id and pos to encode some additional information:
--   When:
--     queue_id = -1 the val column is the collection_id, otherwise collection_id == igroup_id
--     pos == -1     the queue is an infinite sequence where the val column contains the padding value.
--     pos == -2     the val column is a queue number to which consumption requests are redirected.
create table semantic_inputvalues (
       igroup_id integer,                       -- ID for the input group; non-unique
       queue_id integer references semantic_input_queues(id),
       pos integer,                             -- position of this value within its queue (some positions are special)
       val bigint,				-- the 64-bit unsigned value
       cmd bigint references semantic_history(hashkey) -- command that created this row
);

-- An output value is a value produced as output when a specimen function is "executed" during fuzz testing.  Each execution of
-- a function produces zero or more output values which are grouped together into a output group.  The values in an output
-- group have a position within the group, although the analysis can be written to treat output groups as either vectors or
-- sets.
create table semantic_outputvalues (
       hashkey bigint,                          -- output set to which this value belongs; non-unique
       vtype character,                         -- V=>value, F=>fault, C=>function call, S=>syscall
       pos integer,                             -- position of value within its output group and vtype pair
       val bigint                               -- value, fault ID, or function ID depending on vtype
       -- command ID is not stored here because this table is already HUGE! One has a reasonably high chance of
       -- figuring out the command ID by looking at the semantic_fio table.
);

-- Some output values indicate special situations described by this table. See 00-create-schema.C for how this table
-- is populated.
create table semantic_faults (
       id integer primary key,                  -- the special integer value
       name varchar(16),                        -- short identifying name
       description text                         -- full description
);

-- List of files. The other tables store file IDs rather than file names.
create table semantic_files (
       id integer primary key,                  -- unique positive file ID
       name text,                               -- file name
       digest varchar(40),                      -- SHA1 digest if the file is stored in the semantic_binaries table
       ast varchar(40)                          -- SHA1 digest of the binary AST if one is stored in the database
);

-- Associatations between a specimen and its dynamic libraries.  A "specimen" is a file that appeared as an argument
-- to the 11-add-functions tool; the file_id is any other file (binary or otherwise) that's used by the specimen.
create table semantic_specfiles (
       specimen_id integer references semantic_files(id),
       file_id integer references semantic_files(id)
);

-- A function is the unit of code that is fuzz tested.
create table semantic_functions (
       id integer primary key,                  -- unique function ID
       entry_va bigint,                         -- unique starting virtual address within the binary specimen
       name text,                               -- name of function if known
       file_id integer references semantic_files(id), -- binary file in which function exists, specimen or shared library
       specimen_id integer references semantic_files(id), -- file specified in frontend() call
       ninsns integer,                          -- number of instructions in function
       isize integer,                           -- size of function instructions in bytes, non-overlapping
       dsize integer,                           -- size of function data in bytes, non-overlapping
       size integer,                            -- total size of function, non-overlapping
       digest varchar(40),                      -- SHA1 hash of the function's instructions and static data
       cmd bigint references semantic_history(hashkey) -- command that created this row
);

-- Function call graph
create table semantic_cg (
       caller integer references semantic_functions(id),
       callee integer references semantic_functions(id),
       file_id integer references semantic_files(id), -- File from which this call originates (used during update)
       cmd bigint references semantic_history(hashkey) -- Command that created this row
);

-- This is the list of instructions for each function.  Functions need not be contiguous in memory, and two instructions
-- might overlap, which is why we have to store each instruction individually.  Every instruction belongs to exactly one
-- function.
create table semantic_instructions (
       address bigint,                          -- virtual address for start of instruction
       size integer,                            -- size of instruction in bytes
       assembly text,                           -- unparsed instruction including hexadecimal address
       func_id integer references semantic_functions(id), --function to which this instruction belongs
       position integer,                        -- zero-origin index of instruction within function
       src_file_id integer,                     -- source code file that produced this instruction, or -1
       src_line integer,                        -- source line number that produced this instruction, or -1
       cmd bigint references semantic_history(hashkey) -- command that created this row
);

-- Table that holds base-64 encoded binary information.  Large binary information is split into smaller, more manageable
-- chunks that are reassembled according to the 'pos' column.  The key used to look up data is a 20-byte (40 hexadecimal
-- characters) SHA1 computed across all the chunks.
create table semantic_binaries (
       hashkey character(40),                   -- SHA1 hash across all the chunks with this hashkey
       cmd bigint references semantic_history(hashkey), -- command that created this row
       pos integer,                             -- chunk number for content
       chunk text                               -- base-64 encoding of the binary data
);

-- List of source code.
create table semantic_sources (
       cmd bigint references semantic_history(hashkey), -- command that created this row
       file_id integer references semantic_files(id),
       linenum integer,                         -- one-origin line number within file
       line text                                -- one line from a source file
);

-- Function input/output. One of these is produced each time we fuzz-test a function.
create table semantic_fio (
       func_id integer references semantic_functions(id),
       igroup_id integer,                       -- references semantic_inputvalues.id
       arguments_consumed integer,		-- number of inputs consumed from the arguments queue
       locals_consumed integer,			-- number of inputs consumed form the locals queue
       globals_consumed integer,		-- number of inputs consumed from the globals queue
       functions_consumed integer,		-- number of return values from black-box (skipped-over) functions
       pointers_consumed integer,               -- number of pointers from the inputgroup consumed by this test
       integers_consumed integer,               -- number of integers from the inputgroup consumed by this test
       instructions_executed integer,           -- number of instructions executed by this test
       ogroup_id bigint,                        -- output produced by this function, semantic_outputvalues.hashkey
       status integer references semantic_faults(id), -- exit status of the test
       elapsed_time double precision,           -- number of seconds elapsed excluding ptr analysis
       cpu_time double precision,               -- number of CPU seconds used excluding ptr analysis
       cmd bigint references semantic_history(hashkey) -- command that created this row
);

-- This table describes the kinds of events that can happen while a function is running. The events themselves are
-- stored in semantic_fio_trace. See 00-create-schema.C for how this table is populated.
create table semantic_fio_events (
       id integer primary key,
       name varchar(16),                        -- short name of event
       description text                         -- full event description
);

-- This table contains trace info per test.  Not all tests generate trace info, and the contents of this table is
-- mostly for debugging what happened during a test.
create table semantic_fio_trace (
       func_id integer, -- (need for speed) references semantic_functions(id),
       igroup_id integer,                       -- references semantic_inputvalues.id
       pos integer,                             -- sequence number of this event within this test
       addr bigint,                             -- specimen virtual address where event occurred
       event_id integer, --(need for speed) references semantic_fio_events(id),
       minor integer,				-- event minor number (usually zero)
       val bigint				-- event value, interpretation depends on event_id
);

-- Function similarity--how similar are pairs of functions.  The relation_id allows us to store multiple function similarity
-- relationships where all the similarity values for a given relationship were calculated the same way.  E.g., relationship
-- #0 could be computed as the maximum output group similarity while relationship #1 could be the average.
create table semantic_funcsim (
       func1_id integer references semantic_functions(id),
       func2_id integer references semantic_functions(id), -- func1_id < func2_id
       similarity double precision,             -- a value between 0 and 1, with one being equality
       ncompares integer,                       -- number of output groups compared to reach this value
       maxcompares integer,                     -- potential number of comparisons possible (ncompares is a random sample)
       relation_id int,                         -- Identying number for this set of function similarity values (default is zero)
       cmd bigint references semantic_history(hashkey) -- command that set the precision on this row
);

-- Clusters. The commands allow many different cluster tables to be created. This is the template for creating
-- those tables.
create table semantic_clusters_tpl (
       id integer,                              -- cluster ID number
       func_id integer references semantic_functions(id)
);
