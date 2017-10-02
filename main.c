
#include <pfs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opt.h"

static void usage()
{
    printf(
       /*                                                                                */
        "Usage: pfs [OPTIONS] [FILE]\n"
        "\n"
        "  -l, --list           List the names of all files contained in [FILE]\n"
        "  -s, --sizes          Show sizes in bytes for all printed files\n"
        "  -h, --human          Use human-readable values for file sizes (e.g. KiB, MiB)\n"
        "  -e, --extract <name> Extract <name> into the current working directory\n"
        "  -o, --output <name>  Write the contents of <name> to stdout\n"
        "  -r, --remove <name>  Remove <name> from [FILE]\n"
        "  -i, --insert <path>  Insert the file from <path> into [FILE]\n"
        "  -w, --write <name>   Read from stdin to insert a file with <name> into [FILE]\n"
        "  -c, --create <path>  Create a new, empty PFS archive at <path>\n"
        "      --help           Display this dialog\n"
    );
}

static int open_error(int rc, const char* path)
{
    fprintf(stderr, "Error: ");
    
    switch (rc)
    {
    case PFS_NOT_FOUND:
        fprintf(stderr, "no file found at '%s'\n", path);
        break;
    
    case PFS_OUT_OF_MEMORY:
        fprintf(stderr, "out of memory while opening '%s'\n", path);
        break;
    
    case PFS_FILE_ERROR:
        fprintf(stderr, "read operation failed for '%s'\n", path);
        break;
    
    case PFS_CORRUPTED:
        fprintf(stderr, "file is not a valid PFS archive: '%s'\n", path);
        break;
    
    case PFS_COMPRESSION_ERROR:
        fprintf(stderr, "decompression failure while reading '%s'\n", path);
        break;
    
    default:
        fprintf(stderr, "unknown failure code\n");
        break;
    }
    
    return rc;
}

static void print_bytes_human(uint32_t bytes)
{
    uint32_t rem = 0;
    int i = 0;
    
    while (bytes >= 1024)
    {
        rem = bytes % 1024;
        bytes /= 1024;
        i++;
    }
    
    if (i == 0)
    {
        printf("%6u B   ", bytes);
    }
    else
    {
        const char* units;
        
        printf("%4u.%1u ", bytes, (rem * 10) / 1024);
        
        switch (i)
        {
        case 1:
            units = "KiB";
            break;
        
        case 2:
            units = "MiB";
            break;
        
        case 3:
            units = "GiB";
            break;
        
        default:
            units = "TiB";
            break;
        }
        
        printf("%s ", units);
    }
}

static void pfs_list(PFS* pfs, Opt* opt)
{
    int showBytes = opt_flag(opt, OPT_SIZES);
    int showHuman = opt_flag(opt, OPT_HUMAN);
    uint32_t i = 0;
    
    for (;;)
    {
        const char* name = pfs_file_name(pfs, i);
        uint32_t bytes = pfs_file_size(pfs, i);
        
        if (!name) break;
        
        if (showHuman)
        {
            print_bytes_human(bytes);
        }
        else if (showBytes)
        {
            printf("%10u ", bytes);
        }
        
        printf("%s\n", name);
        i++;
    }
}

static void pfs_file_error(int rc, const char* name, const char* path)
{
    fprintf(stderr, "Error: ");
    
    switch (rc)
    {
    case PFS_NOT_FOUND:
        fprintf(stderr, "could not find '%s' in '%s'\n", name, path);
        break;
    
    case PFS_OUT_OF_MEMORY:
        fprintf(stderr, "out of memory while extracting '%s'\n", name);
        break;
    
    case PFS_COMPRESSION_ERROR:
        fprintf(stderr, "failure while decompressing '%s'\n", name);
        break;
        
    default:
        fprintf(stderr, "unknown failure code\n");
        break;
    }
}

static int pfs_extract_file(PFS* pfs, const char* name, const char* path)
{
    uint8_t* data;
    uint32_t length;
    FILE* fp;
    int rc;
    
    rc = pfs_file_data(pfs, name, &data, &length);
    
    if (rc)
    {
        pfs_file_error(rc, name, path);
        return rc;
    }
    
    fp = fopen(name, "wb+");
    rc = PFS_FILE_ERROR;
    
    if (!fp)
    {
        fprintf(stderr, "Error: could not open '%s' for writing\n", name);
        goto fail_no_file;
    }
    
    if (fwrite(data, sizeof(uint8_t), length, fp) != length)
    {
        fprintf(stderr, "Error: write failure for '%s'\n", name);
        goto fail;
    }
    
    rc = 0;
    printf("Extracted '%s'\n", name);
    
fail:
    fclose(fp);
fail_no_file:
    free(data);
    
    return rc;
}

static int pfs_output_file(PFS* pfs, const char* name, const char* path)
{
    uint8_t* data;
    uint32_t length;
    int rc;
    
    rc = pfs_file_data(pfs, name, &data, &length);
    
    if (rc)
    {
        pfs_file_error(rc, name, path);
        return rc;
    }
    
    if (fwrite(data, sizeof(uint8_t), length, stdout) != length)
    {
        fprintf(stderr, "Error: write failure for '%s'\n", name);
        rc = PFS_FILE_ERROR;
        goto fail;
    }
    
fail:
    free(data);
    
    return rc;
}

static int pfs_extract(PFS* pfs, Opt* opt, const char* path)
{
    uint32_t i;
    
    for (i = 0; i < opt->count; i++)
    {
        OptArg* arg = &opt->args[i];
        
        if (arg->bit == OPT_EXTRACT)
        {
            int rc = pfs_extract_file(pfs, arg->str, path);
            if (rc) return rc;
        }
    }
    
    return 0;
}

static int pfs_output(PFS* pfs, Opt* opt, const char* path)
{
    uint32_t i;
    
    for (i = 0; i < opt->count; i++)
    {
        OptArg* arg = &opt->args[i];
        
        if (arg->bit == OPT_OUTPUT)
        {
            return pfs_output_file(pfs, arg->str, path);
        }
    }
    
    return 0;
}

static const char* filename_from_path(const char* path)
{
    const char* name = path;
    const char* end = path + strlen(path);
    
    while (path < end)
    {
        int c = *path++;
        
        if (c == '/' || c == '\\')
            name = path;
    }
    
    return name;
}

static void pfs_default_info(PFS* pfs, const char* path)
{
    uint32_t total = 0;
    uint32_t totalCompressed = 0;
    uint32_t count = pfs_file_count(pfs);
    uint32_t n, i;
    const char* name;
    
    for (i = 0; i < count; i++)
    {
        uint32_t size = pfs_file_size(pfs, i);
        uint32_t comp = pfs_file_size_compressed(pfs, i);
        
        total += size;
        totalCompressed += comp;
    }
    
    name = filename_from_path(path);
    
    printf("%s\n", name);
    
    n = strlen(name);
    for (i = 0; i < n; i++)
    {
        fputc('-', stdout);
    }
    
    fputc('\n', stdout);
    printf("File count: %u\nCompression ratio: %.1f%%\n", count, (total == 0) ? 0 : 100.0 - ((double)totalCompressed / (double)total) * 100.0);
}

static void pfs_save_error(int rc, const char* path)
{
    fprintf(stderr, "Error: ");
    
    switch (rc)
    {
    case PFS_OUT_OF_MEMORY:
        fprintf(stderr, "out of memory while writing '%s'\n", path);
        break;
    
    case PFS_FILE_ERROR:
        fprintf(stderr, "write operation failed for file '%s'\n", path);
        break;
    
    case PFS_COMPRESSION_ERROR:
        fprintf(stderr, "compression failure while writing '%s'\n", path);
        break;
    
    default:
        fprintf(stderr, "unknown error code\n");
        break;
    }
}

static int pfs_save(PFS* pfs, const char* path)
{
    int rc = pfs_write_to_disk(pfs, path);
        
    if (rc)
    {
        pfs_save_error(rc, path);
        return rc;
    }
    
    printf("Saved '%s'\n", path);
    return PFS_OK;
}

static int pfs_remove_impl(PFS* pfs, const char* name, const char* path)
{
    int rc = pfs_remove_file(pfs, name);
    
    switch (rc)
    {
    case PFS_OK:
        printf("Removing '%s'\n", name);
        break;
    
    case PFS_NOT_FOUND:
        fprintf(stderr, "Error: no file '%s' in '%s'\n", name, path);
        break;
    
    default:
        fprintf(stderr, "Error: unknown error code\n");
        break;
    }
    
    return rc;
}

static int pfs_remove(PFS* pfs, Opt* opt, const char* path)
{
    uint32_t i;
    int count = 0;
    
    for (i = 0; i < opt->count; i++)
    {
        OptArg* arg = &opt->args[i];
        
        if (arg->bit == OPT_REMOVE)
        {
            int rc = pfs_remove_impl(pfs, arg->str, path);
            if (rc) return rc;
            
            count++;
        }
    }
    
    return (count) ? pfs_save(pfs, path) : PFS_OK;
}

static int pfs_insert_impl(PFS* pfs, const char* name, uint8_t* data, uint32_t len)
{
    int rc = pfs_insert_file(pfs, name, data, len);
    
    switch (rc)
    {
    case PFS_OK:
        printf("Inserting '%s'\n", name);
        break;
    
    case PFS_OUT_OF_MEMORY:
        fprintf(stderr, "Error: out of memory while inserting '%s'\n", name);
        break;
    
    case PFS_COMPRESSION_ERROR:
        fprintf(stderr, "Error: compression failed while inserting '%s'\n", name);
        break;
    
    default:
        fprintf(stderr, "Error: unknown error code\n");
        break;
    }
    
    return rc;
}

static int pfs_insert_single(PFS* pfs, const char* filepath)
{
    const char* name;
    uint8_t* data;
    uint32_t len;
    FILE* fp;
    int rc;
    
    fp = fopen(filepath, "rb");
    
    if (!fp)
    {
        open_error(PFS_NOT_FOUND, filepath);
        return PFS_NOT_FOUND;
    }
    
    fseek(fp, 0, SEEK_END);
    len = (uint32_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (len == 0)
    {
        rc = PFS_FILE_ERROR;
        goto fail_no_data;
    }
    
    data = (uint8_t*)malloc(len);
    
    if (!data)
    {
        rc = open_error(PFS_OUT_OF_MEMORY, filepath);
        goto fail_no_data;
    }
    
    if (fread(data, sizeof(uint8_t), len, fp) != len)
    {
        rc = open_error(PFS_FILE_ERROR, filepath);
        goto fail;
    }
    
    name = filename_from_path(filepath);
    rc = pfs_insert_impl(pfs, name, data, len);
    
fail:
    free(data);
fail_no_data:
    fclose(fp);
    return rc;
}

static int pfs_insert(PFS* pfs, Opt* opt, const char* path)
{
    uint32_t i;
    int count = 0;
    
    for (i = 0; i < opt->count; i++)
    {
        OptArg* arg = &opt->args[i];
        
        if (arg->bit == OPT_INSERT)
        {
            int rc = pfs_insert_single(pfs, arg->str);
            if (rc) return rc;
            
            count++;
        }
    }
    
    return (count) ? pfs_save(pfs, path) : PFS_OK;
}

static int pfs_write_file(PFS* pfs, const char* name, const char* path)
{
    uint8_t* data = NULL;
    uint32_t cap = 512;
    uint32_t len = 0;
    uint32_t read;
    int rc = 0;
    
    for (;;)
    {
        if (len + 1024 > cap)
        {
            uint8_t* buf;
            
            cap *= 2;
            buf = (uint8_t*)realloc(data, cap);
            
            if (!buf)
            {
                if (data) free(data);
                fprintf(stderr, "Error: out of memory while reading form stdin\n");
                return PFS_OUT_OF_MEMORY;
            }
            
            data = buf;
        }
        
        read = fread(data + len, sizeof(uint8_t), 1024, stdin);
        len += read;
        
        if (read < 1024)
        {
            if (feof(stdin)) break;
            
            free(data);
            fprintf(stderr, "Error: failure while reading from stdin\n");
            return PFS_FILE_ERROR;
        }
    }
    
    if (data)
    {
        if (len > 0)
        {
            rc = pfs_insert_impl(pfs, name, data, len);
            
            if (rc == PFS_OK)
            {
                rc = pfs_save(pfs, path);
            }
        }
        
        free(data);
    }
    
    return rc;
}

static int pfs_write(PFS* pfs, Opt* opt, const char* path)
{
    uint32_t i;
    
    for (i = 0; i < opt->count; i++)
    {
        OptArg* arg = &opt->args[i];
        
        if (arg->bit == OPT_WRITE)
        {
            return pfs_write_file(pfs, arg->str, path);
        }
    }
    
    return 0;
}

static void pfs_create_error(int rc, const char* path)
{
    fprintf(stderr, "Error: ");
    
    switch (rc)
    {
    case PFS_OUT_OF_MEMORY:
        fprintf(stderr, "out of memory while creating '%s'\n", path);
        break;
    
    default:
        fprintf(stderr, "unknown error code\n");
        break;
    }
}

static int pfs_create_file(const char* path)
{
    PFS* pfs;
    FILE* fp;
    int rc;
    
    fp = fopen(path, "rb");
    
    if (fp)
    {
        fclose(fp);
        fprintf(stderr, "Error: a file already exists at '%s'\n", path);
        return PFS_FILE_ERROR;
    }
    
    rc = pfs_create_new(&pfs);
    
    if (rc)
    {
        pfs_create_error(rc, path);
        return rc;
    }
    
    rc = pfs_save(pfs, path);
    pfs_close(pfs);
    return rc;
}

static int pfs_create(Opt* opt)
{
    uint32_t i;
    
    for (i = 0; i < opt->count; i++)
    {
        OptArg* arg = &opt->args[i];
        
        if (arg->bit == OPT_CREATE)
        {
            int rc = pfs_create_file(arg->str);
            if (rc) return rc;
        }
    }
    
    return 0;
}

int main(int argc, const char** argv)
{
    const char* path;
    PFS* pfs;
    Opt opt;
    int rc = EXIT_SUCCESS;
    
    static const OptHandler optHandlers[] = {
        { "l",          OPT_LIST    },
        { "list",       OPT_LIST    },
        { "s",          OPT_SIZES   },
        { "sizes",      OPT_SIZES   },
        { "h",          OPT_HUMAN   },
        { "human",      OPT_HUMAN   },
        { "e",          OPT_EXTRACT },
        { "extract",    OPT_EXTRACT },
        { "o",          OPT_OUTPUT  },
        { "output",     OPT_OUTPUT  },
        { "r",          OPT_REMOVE  },
        { "remove",     OPT_REMOVE  },
        { "i",          OPT_INSERT  },
        { "insert",     OPT_INSERT  },
        { "w",          OPT_WRITE   },
        { "write",      OPT_WRITE   },
        { "c",          OPT_CREATE  },
        { "create",     OPT_CREATE  },
        { "help",       OPT_HELP    },
        { NULL,         0           }
    };
    
    if (argc < 2)
    {
        usage();
        return EXIT_SUCCESS;
    }
    
    rc = opt_process(&opt, optHandlers, argc, argv);
    if (rc) return EXIT_FAILURE;
    
    if (opt_flag(&opt, OPT_HELP))
    {
        usage();
        goto done_opt;
    }
    
    if (opt_flag(&opt, OPT_CREATE))
    {
        rc = pfs_create(&opt);
        goto done_opt;
    }
    
    path = argv[argc - 1];
    opt.count--;
    rc = pfs_open(&pfs, path);
    
    if (rc)
    {
        open_error(rc, path);
        rc = EXIT_FAILURE;
        goto done_opt;
    }
    
    if (opt_flag(&opt, OPT_LIST))
    {
        pfs_list(pfs, &opt);
        goto done;
    }
    
    if (opt_flag(&opt, OPT_EXTRACT))
    {
        rc = pfs_extract(pfs, &opt, path);
        goto done;
    }
    
    if (opt_flag(&opt, OPT_OUTPUT))
    {
        rc = pfs_output(pfs, &opt, path);
        goto done;
    }
    
    if (opt_flag(&opt, OPT_REMOVE))
    {
        rc = pfs_remove(pfs, &opt, path);
        goto done;
    }
    
    if (opt_flag(&opt, OPT_INSERT))
    {
        rc = pfs_insert(pfs, &opt, path);
        goto done;
    }
    
    if (opt_flag(&opt, OPT_WRITE))
    {
        rc = pfs_write(pfs, &opt, path);
        goto done;
    }
    
    pfs_default_info(pfs, path);
    
done:
    pfs_close(pfs);
done_opt:
    opt_free(&opt);
    return rc;
}
