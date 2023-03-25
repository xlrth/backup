# summary

Command line backup tool, featuring:
- Preserving of history of old backups.
- File-system-based, using hard links for deduplication, backuped snapshots and their files are regular directories and files in their original structure, and can thus be quickly restored or deleted using normal file system operations.
- Management and verification functionality, allowing consistency checks or aid in reviewing before deletion.


# manual

BACKUP

Description:

    Creates copies of files and directories, specified in a configuration file.
    Copies are created in a new sub-directory (snapshot) in the specified
    repository directory. All already existing snapshots in the same repository
    are used for deduplication to reduce required hard disk space.

Methods:

    If the file to be backuped is already part of a snapshot, i.e., an identical
    file is found in the repository, the copy operation is replaced with a
    hard link operation. A hard link operation creates a normal file that has
    shared content with one or multiple other files. In this case, the new
    backup file shares content with at least one other backup file.
    The search for an existing backup is a two-step process:
    1. A file with same full path name, size, and modification time is searched.
    2. If no file was found, a SHA256 hash is calculated and searched.
    Both searches are done using a file table in each snapshot in the form of a
    sqlite database.

    Hash calculation can be enforced by specifying --always_hash. This option
    may increase backup duration significantly, but might also reveal file
    changes that did not affect modification time. Please note that files with
    similar signature (used file properties of search step 1) but different
    hashes can (at the moment) NOT be handled and will be skipped.
    --always_hash is therefore usable only for revealing those files. They can
    be backuped after their modification time was changed.

    Files smaller than a file-system-dependent threshold are never hard-linked,
    but added via a copy operation.

Configuration File Format (Windows example):

    * lines starting with "*" are ignored
    [sources]
    C:\
    C:\Data\OneSpecificFile.txt
    ..\RelativeDataPath

    * the "excludes" section specifies PATH SUFFIXES of files and directories
    * to be excluded in the backup process
    [excludes]
    C:\Windows
    :\pagefile.sys
    \thumbs.db
    .tmp
    _NO_BACKUP

    The configuration file is interpreted as UTF-8.

Restoring:

    Restoring is done by manually copying files/directories from a snapshot
    directory to the desired target.

    CAUTION: When restoring files, be sure to make copies (rather than a move
    operation within the same partition) to eliminate all hard links.
    Otherwise modification of restored files may lead to modification of other
    restored files or backuped files.

Partial or full deletion of snapshots:

    Full deletion of snapshots can be done by deleting the snapshot directory.
    The DISTILL command can be used for distilling/extracting unique files.
    Partial deletion can be done by manual file/directory deletion and
    subsequent execution of the PURGE command.

Restrictions:

    Following symbolic links or backing up links themselves is not supported.
    Symbolic links in source directory trees will be excluded from backup.

Path arguments:

    <source-config-file>    Path to a configuration file, specifiying files or
                            directories to backup, as well as a blacklist with
                            suffixes of file/directory paths to exclude.
                            See also section "Configuration File Format".

    <repository-dir>        Target directory on the backup storage. Existing
                            snapshots in the same directory are used for
                            deduplication. The repository directory must not
                            contain any directories other than snapshots.

Options:

    --help          Displays this help text.

    --verbose       Higher verbosity of command line logging.

    --incremental   Skips unchanged files: Files will not be added to the newly
                    created snapshot, if their signature, i.e. full path name,
                    size and modification time, can be found in any snapshot in
                    the repository. This option will reduce backup duration.

    --always_hash   Enables calculcation of hash of all files to be backuped.
                    This option may increase backup duration significantly.
                    See also section "Methods".

    --suffix=s      Adds the suffix s to the directory name of the new snapshot.
                    This option can be used to mark spapshots, for example to
                    distinguish full snapshots from incremental ones.