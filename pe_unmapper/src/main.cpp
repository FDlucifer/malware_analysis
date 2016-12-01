#include <windows.h>
#include <stdio.h>

#include "pe_virtual_to_raw.h"
#include "relocate.h"

void hexdump(BYTE *buf, size_t size)
{
    for (int i = 0; i < size; i++) {
        printf("%02x ",buf[i]);
    }
    printf("\n");
}

bool remap_pe_file(IN const char* filename, IN const char* out_filename, ULONGLONG loadBase)
{
    if (filename == NULL || out_filename == NULL) return false;
    printf("filename: %s\n", filename);
    FILE *f = fopen(filename, "rb");
    if (!f) {
        printf("Cannot open file!\n");
        return false;
    }

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    printf("size = %d\n", size);
    BYTE* in_buf = (BYTE*) VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    BYTE* out_buf = (BYTE*) VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    fseek(f, 0, SEEK_SET);
    fread(in_buf, 1, size, f);
    fclose(f);

    BYTE* nt_headers = get_nt_hrds(in_buf);
    if (nt_headers == NULL) {
        printf("Invalid payload - it is not a PE file!\n", in_buf);
        return false;
    }

    ULONGLONG oldBase = 0;
    bool is_payload_64b = is64bit((BYTE*) in_buf);

    if (is_payload_64b) {
        IMAGE_NT_HEADERS64* nt_headers64 = (IMAGE_NT_HEADERS64*) nt_headers;
        oldBase = nt_headers64->OptionalHeader.ImageBase;
    } else {
        IMAGE_NT_HEADERS32* nt_headers32 = (IMAGE_NT_HEADERS32*) nt_headers;
        oldBase = nt_headers32->OptionalHeader.ImageBase;
    }

    bool isOk = true;
    printf("Load Base: %llx\n", loadBase);
    printf("Old Base: %llx\n", oldBase);

    if (loadBase != 0 && loadBase != oldBase) {
        if (!apply_relocations(oldBase, loadBase, in_buf, size)) {

            printf("Could not relocate, changing the image base instead...\n");
            if (is_payload_64b) {
                IMAGE_NT_HEADERS64* nt_headers64 = (IMAGE_NT_HEADERS64*) nt_headers;
                nt_headers64->OptionalHeader.ImageBase = loadBase;
            } else {
                IMAGE_NT_HEADERS32* nt_headers32 = (IMAGE_NT_HEADERS32*) nt_headers;
                nt_headers32->OptionalHeader.ImageBase = static_cast<DWORD>(loadBase);
            }
            
        } else {
            printf("Relocations applied!\n");
        }
    }
    SIZE_T raw_size = 0;
    if (!sections_virtual_to_raw(in_buf, size, out_buf, &raw_size)) isOk = false;
    
    f = fopen(out_filename, "wb");
    if (f) {
        fwrite(out_buf, 1, raw_size, f);
        fclose(f);
    }

    VirtualFree(in_buf, size, MEM_FREE);
    VirtualFree(out_buf, size, MEM_FREE);
    return isOk;
}

int main(int argc, char *argv[])
{
    char*  filename = NULL;
    char* out_filename = "out.exe";
    ULONGLONG loadBase = 0;
    if (argc < 3) {
        printf("Required args: <input file> <load base: in hex> [*output file]\n");
        printf("* - optional\n");
        system("pause");
        return -1;
    }
    filename = argv[1];
    if (sscanf(argv[2],"%llX", &loadBase) == 0) {
        sscanf(argv[2],"%#llX", &loadBase);
    }

    if (argc > 3) {
        out_filename = argv[3];
    }

    if (remap_pe_file(filename, out_filename, loadBase)) {
        printf("Success!\n");
        printf("Saved output to: %s\n", out_filename);
    } else {
        printf("Failed!\n");
    }
    system("pause");
    return 0;
}
