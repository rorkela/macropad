#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "json_parser.h"
#include "gui.h"

#if defined(_WIN32) || defined(_WIN64)
#define NULL_DEVICE "NUL"
#else
#define NULL_DEVICE "/dev/null"
#endif

void print_usage(const char *prog_name)
{
    printf("Macro Pad Layout JSON Compiler\n");
    printf("Usage: %s [options] [input_file.json]\n\n", prog_name);
    printf("Options:\n");
    printf("  -h, --help           Show this help message and exit\n");
    printf("  -c, --compile-only   Only generate 'mappings.bin' file without flashing via dfu-util\n");
    printf("  -v, --verbose        Show process logs during execution\n\n");
    printf("  -g, --gui            Open GUI\n\n");
    printf("Arguments:\n");
    printf("  [input_file.json]    Path to layout JSON (defaults to 'layout.json')\n");
}

int main(int argc, char *argv[])
{
    const char *input_filename = "layout.json";
    bool compile_only = false;
    bool verbose = false;
    bool gui = false;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
        {
            print_usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[i], "--compile-only") == 0 || strcmp(argv[i], "-c") == 0)
        {
            compile_only = true;
        }
        else if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0)
        {
            verbose = true;
        }
        else if (strcmp(argv[i], "--gui") == 0 || strcmp(argv[i], "-g") == 0)
        {
            gui=true;
        }
        else
        {
            input_filename = argv[i];
        }
    }

    printf("Loading layout config from: %s\n", input_filename);
    if(gui) call_gui(input_filename);

    mappings_t *map = NULL;
    size_t total_bin_size = 0;

    if (parse_json_layout(input_filename, &map, &total_bin_size) != 0)
    {
        printf("Failed to parse JSON file layout configurations. Run '%s --help' for template guide.\n", argv[0]);
        return 1;
    }

    const char *output_filename = compile_only ? "mappings.bin" : ".tmp_mappings.bin";
    FILE *outfile = fopen(output_filename, "wb");
    if (!outfile)
    {
        perror("Failed to generate file output binary target");
        free(map);
        return 1;
    }

    size_t written = fwrite(map, 1, total_bin_size, outfile);
    fclose(outfile);
    free(map);

    if (compile_only)
    {
        printf("Successfully compiled layout structure!\n");
        printf("Total Binary Payload Size: %zu bytes\n", written);
        printf("Flash Output Location Target: 0x%08X\n", MAPPINGS_ADDR);
    }
    else
    {
        printf("Binary prepared (%zu bytes). Invoking dfu-util...\n", written);

        char dfu_command[256];
        if (verbose)
        {
            snprintf(dfu_command, sizeof(dfu_command), "dfu-util -d 1209:1253 -a 1 -D %s", output_filename);
        }
        else
        {
            snprintf(dfu_command, sizeof(dfu_command), "dfu-util -d 1209:1253 -a 1 -D %s > %s 2>&1", output_filename,
                     NULL_DEVICE);
        }

        if (verbose)
            printf("Executing: %s\n", dfu_command);

        int status = system(dfu_command);

        remove(output_filename);

        if (status == 0)
        {
            printf("\nFlash complete! Layout successfully deployed to macro pad.\n");
        }
        else
        {
            fprintf(stderr, "\nError: dfu-util flashing failed (Exit code: %d).\n", status);
            fprintf(stderr, "Ensure your macro pad is in DFU bootloader mode and try again.\n");
            return 1;
        }
    }

    return 0;
}
