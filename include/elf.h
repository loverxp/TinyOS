#ifndef ELF_H
#define ELF_H

#include "types.h"

// ELF magic number: "\x7FELF"
#define ELF_MAGIC       0x464C457F

// ELF class (e_class)
#define ELFCLASS32      1       // 32-bit

// ELF data encoding (e_data)
#define ELFDATA2LSB     1       // Little endian

// ELF type (e_type)
#define ET_EXEC         2       // Executable file

// Machine (e_machine)
#define EM_386          3       // Intel 80386

// Program header type (p_type)
#define PT_NULL         0
#define PT_LOAD         1       // Loadable segment

// Program header flags (p_flags)
#define PF_X            1       // Execute
#define PF_W            2       // Write
#define PF_R            4       // Read

// ELF32 header structure - 52 bytes
typedef struct {
    uint32_t    e_magic;        // ELF magic (0x7F 'E' 'L' 'F')
    uint8_t     e_class;        // 1 = 32-bit
    uint8_t     e_data;         // 1 = little endian
    uint8_t     e_version;      // must be 1
    uint8_t     e_osabi;        // 0 = System V
    uint8_t     e_abiver;       // 0
    uint8_t     e_pad[7];       // padding
    uint16_t    e_type;         // 2 = executable
    uint16_t    e_machine;      // 3 = i386
    uint32_t    e_version2;     // must be 1
    uint32_t    e_entry;        // entry point virtual address
    uint32_t    e_phoff;        // program header table offset
    uint32_t    e_shoff;        // section header table offset
    uint32_t    e_flags;        // processor-specific flags
    uint16_t    e_ehsize;       // ELF header size (52 for 32-bit)
    uint16_t    e_phentsize;    // program header entry size (32 for 32-bit)
    uint16_t    e_phnum;        // number of program header entries
    uint16_t    e_shentsize;    // section header entry size
    uint16_t    e_shnum;        // number of section header entries
    uint16_t    e_shstrndx;     // section header string table index
} __attribute__((packed)) Elf32_Ehdr;

// ELF32 program header structure - 32 bytes
typedef struct {
    uint32_t    p_type;         // PT_LOAD = 1
    uint32_t    p_offset;       // offset in file
    uint32_t    p_vaddr;        // virtual address to load at
    uint32_t    p_paddr;        // physical address (unused)
    uint32_t    p_filesz;       // size in file
    uint32_t    p_memsz;        // size in memory (may include BSS)
    uint32_t    p_flags;        // PF_R, PF_W, PF_X
    uint32_t    p_align;        // alignment
} __attribute__((packed)) Elf32_Phdr;

// Validate ELF header: returns 1 if valid 32-bit executable for i386
static inline int elf_validate(const Elf32_Ehdr* hdr) {
    return hdr->e_magic   == ELF_MAGIC
        && hdr->e_class   == ELFCLASS32
        && hdr->e_data    == ELFDATA2LSB
        && hdr->e_type    == ET_EXEC
        && hdr->e_machine == EM_386;
}

#endif // ELF_H