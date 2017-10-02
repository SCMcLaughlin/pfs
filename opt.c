
#include "opt.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define opt_is_pow2_or_zero(n) ((n == 0) || (((n) & ((n) - 1)) == 0))

static int opt_handle(Opt* opt, const OptHandler* handlers, const char* arg, int* prevBit)
{
    for (;;)
    {
        const char* name = handlers->name;
        
        if (name == NULL)
            break;
        
        if (strcmp(arg, name) == 0)
        {
            *prevBit = handlers->bit;
            opt->flags |= (1 << handlers->bit);
            return 0;
        }
        
        handlers++;
    }
    
    fprintf(stderr, "Error: unknown option '%s%s'\n", (arg[1] == 0) ? "-" : "--", arg);
    return 1;
}

int opt_process(Opt* opt, const OptHandler* handlers, int argc, const char** argv)
{
    char shortOpt[2] = {0, 0};
    int i, rc;
    int prevBit = -1;
    
    memset(opt, 0, sizeof(Opt));
    
    for (i = 1; i < argc; i++)
    {
        const char* arg = argv[i];
        
        if (!arg) break;
        
        if (arg[0] == '-')
        {
            /* Long option? */
            if (arg[1] == '-')
            {
                rc = opt_handle(opt, handlers, &arg[2], &prevBit);
                if (rc) return rc;
                
                continue;
            }
            
            /* Short option(s) */
            for (;;)
            {
                arg++;
                
                if (*arg == 0)
                    break;
                
                shortOpt[0] = *arg;
                
                rc = opt_handle(opt, handlers, shortOpt, &prevBit);
                if (rc) return rc;
            }
            
            continue;
        }
        
        if (prevBit == -1)
            continue;
        
        /* Arguments */
        if (opt_is_pow2_or_zero(opt->count))
        {
            uint32_t cap = (opt->count == 0) ? 1 : (opt->count * 2);
            OptArg* args = (OptArg*)realloc(opt->args, sizeof(OptArg) * cap);
            
            if (!args)
            {
                fprintf(stderr, "Error: out of memory while processing arguments\n");
                return 1;
            }
            
            opt->args = args;
        }
        
        opt->args[opt->count].str = arg;
        opt->args[opt->count].bit = prevBit;
        opt->count++;
    }
    
    return 0;
}

void opt_free(Opt* opt)
{
    if (opt->args)
    {
        free(opt->args);
        opt->args = NULL;
    }
}
