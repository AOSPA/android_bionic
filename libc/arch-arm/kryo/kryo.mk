libc_openbsd_src_files_exclude_arm += \
    upstream-openbsd/lib/libc/string/memmove.c \
    upstream-openbsd/lib/libc/string/stpcpy.c \
    upstream-openbsd/lib/libc/string/strcat.c \

libc_bionic_src_files_exclude_arm += \
    arch-arm/generic/bionic/memcpy.S \
    arch-arm/generic/bionic/memset.S \
    arch-arm/generic/bionic/strcmp.S \
    arch-arm/generic/bionic/strcpy.S \
    arch-arm/generic/bionic/strlen.c \

libc_bionic_src_files_arm += \
    arch-arm/kryo/bionic/memcpy.S \
    arch-arm/kryo/bionic/memmove.S \
    arch-arm/krait/bionic/strcmp.S

# Use cortex-a15 versions of strcat/strcpy/strlen and standard memmove
libc_bionic_src_files_arm += \
    arch-arm/cortex-a15/bionic/stpcpy.S \
    arch-arm/cortex-a15/bionic/strcat.S \
    arch-arm/cortex-a15/bionic/strcpy.S \
    arch-arm/cortex-a15/bionic/strlen.S \

# Use cortex-a53 memset
libc_bionic_src_files_arm += \
    arch-arm/cortex-a7/bionic/memset.S \
