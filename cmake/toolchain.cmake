set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv32)

set(TOOLCHAIN_PREFIX riscv-none-elf)

set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_OBJCOPY      ${TOOLCHAIN_PREFIX}-objcopy)
set(CMAKE_AR           ${TOOLCHAIN_PREFIX}-gcc-ar)
set(CMAKE_RANLIB       ${TOOLCHAIN_PREFIX}-gcc-ranlib)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(ARCH_FLAGS "-march=rv32imc_zicsr_zifencei -mabi=ilp32 -mcmodel=medlow")

set(CMAKE_C_FLAGS_INIT "${ARCH_FLAGS} -Os -fstrict-volatile-bitfields -fno-strict-aliasing -fno-common -fno-builtin-printf -ffreestanding -flto")

set(CMAKE_ASM_FLAGS_INIT "${ARCH_FLAGS}")

set(CMAKE_EXE_LINKER_FLAGS_INIT "${ARCH_FLAGS} -nostdlib -nostartfiles -Wl,-Bstatic,--gc-sections,--print-memory-usage")
