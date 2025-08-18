cmake_minimum_required(VERSION 3.23)

function(print_var var_name)
    message(STATUS "${var_name}: ${${var_name}}")
endfunction()

function(print_list list_name)
    message(STATUS "${list_name}:")
    foreach(item ${${list_name}})
        message(STATUS "  ${item}")
    endforeach()
endfunction()

# --------------------------------- Toolchain -------------------------------- #
# Specify gcc-arm-none-eabi toolchain
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER  arm-none-eabi-gcc)
set(CMAKE_AR arm-none-eabi-ar)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_OBJDUMP arm-none-eabi-objdump)
set(SIZE arm-none-eabi-size)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# ---------------------------------- Options --------------------------------- #
set(project_name "py32app" CACHE STRING "Project name")
set(mcu "PY32F002Ax5" CACHE STRING "MCU Type")
# MCU types: 
#   PY32F002Ax5
#   PY32F002Bx5
#   PY32F003x4, PY32F003x6, PY32F003x8,
#   PY32F030x6, PY32F030x8, 
#   PY32F072xB
string(TOUPPER ${mcu} mcu_upper)
string(TOLOWER ${mcu} mcu_lower)

option(use_ll_lib "Use LL library instead of HAL" ON)
option(use_printf_float "Use printf with float" OFF)
option(use_freertos "Use FreeRTOS" OFF)
option(use_dsp "Use DSP library" OFF)
option(use_epd "Use EPD" OFF)
set(flash_program "pyocd" CACHE STRING "Flash program")

# ---------------------------------- Project --------------------------------- #
project(${project_name} C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(lib "${CMAKE_SOURCE_DIR}/Libs/Libraries")

# Compiler options
set(arch_flags -mthumb -mcpu=cortex-m0plus -mfloat-abi=soft)
add_compile_options(${arch_flags})
add_compile_options(-Wall -fdata-sections -ffunction-sections)
add_compile_definitions(${mcu})

# Linker options
set(linker_script "${lib}/LDScripts/${mcu_lower}.ld")
add_link_options(-Wl,-gc-sections,--print-memory-usage,-Map=${PROJECT_BINARY_DIR}/${PROJECT_NAME}.map)
add_link_options(${arch_flags})
add_link_options(-specs=nano.specs)
add_link_options(-T${linker_script})
if (${use_printf_float})
    add_link_options(-u _printf_float)
endif()

# --------------------------------- User Code -------------------------------- #
set(src_dirs "Src")
set(src_files "")
set(inc_dirs "Inc")
set(libs "c" "m" "nosys")

# ----------------------------------- CMSIS ---------------------------------- #
list(APPEND inc_dirs "${lib}/CMSIS/Core/Include")
list(APPEND inc_dirs "${lib}/CMSIS/Device/PY32F0xx/Include")

# --------------------------------- LL or HAL -------------------------------- #
if (${mcu} MATCHES "PY32F002B") 
    set(system_c "py32f002b")
    set(lib_prefix "PY32F002B")
    set(startup "py32f002b")
elseif (${mcu} MATCHES "PY32F07")
    set(system_c "py32f07x")
    set(lib_prefix "PY32F07x")
    set(startup "py32f072")
    if (use_ll_lib)
        message(FATAL_ERROR "LL library not supported for ${mcu_upper}")
    endif()
else()
    set(system_c "py32f0xx")
    set(lib_prefix "PY32F0xx")
    if (${mcu} MATCHES "PY32F002A")
        set(startup "py32f002a")
    elseif (${mcu} MATCHES "PY32F003")
        set(startup "py32f003")
    elseif (${mcu} MATCHES "PY32F030")
        set(startup "py32f030")
    else()
        message(FATAL_ERROR "MCU not supported")
    endif()
endif()

if (${use_ll_lib})
    set(lib_prefix "${lib_prefix}_LL")
    add_compile_definitions(USE_FULL_LL_DRIVER)
else()
    set(lib_prefix "${lib_prefix}_HAL")
endif()

list(APPEND src_files "${lib}/CMSIS/Device/PY32F0xx/Source/system_${system_c}.c")
list(APPEND src_files "${lib}/CMSIS/Device/PY32F0xx/Source/gcc/startup_${startup}.s")
list(APPEND inc_dirs "${lib}/${lib_prefix}_Driver/Inc")
list(APPEND src_dirs "${lib}/${lib_prefix}_Driver/Src")
list(APPEND inc_dirs "${lib}/${lib_prefix}_BSP/Inc")
list(APPEND src_dirs "${lib}/${lib_prefix}_BSP/Src")

# --------------------------------- FreeRTOS --------------------------------- #
if (${use_freertos})
    list(APPEND src_dirs "${lib}/FreeRTOS/")
    list(APPEND src_dirs "${lib}/FreeRTOS/portable/GCC/ARM_CM0")
    list(APPEND inc_dirs "${lib}/FreeRTOS/include")
    list(APPEND inc_dirs "${lib}/FreeRTOS/portable/GCC/ARM_CM0")
    list(APPEND src_files "${lib}/portable/MemMang/heap_4.c")
endif()

# --------------------------------- CMSIS-DSP -------------------------------- #
if (${use_dsp})
    list(APPEND src_files
        "${lib}/CMSIS/DSP/Source/BasicMathFunctions/BasicMathFunctions.c"   
		"${lib}/CMSIS/DSP/Source/BayesFunctions/BayesFunctions.c"   
		"${lib}/CMSIS/DSP/Source/CommonTables/CommonTables.c"   
		"${lib}/CMSIS/DSP/Source/ComplexMathFunctions/ComplexMathFunctions.c"   
		"${lib}/CMSIS/DSP/Source/ControllerFunctions/ControllerFunctions.c" 
		"${lib}/CMSIS/DSP/Source/DistanceFunctions/DistanceFunctions.c" 
		"${lib}/CMSIS/DSP/Source/FastMathFunctions/FastMathFunctions.c" 
		"${lib}/CMSIS/DSP/Source/FilteringFunctions/FilteringFunctions.c"   
		"${lib}/CMSIS/DSP/Source/InterpolationFunctions/InterpolationFunctions.c"   
		"${lib}/CMSIS/DSP/Source/MatrixFunctions/MatrixFunctions.c" 
		"${lib}/CMSIS/DSP/Source/QuaternionMathFunctions/QuaternionMathFunctions.c" 
		"${lib}/CMSIS/DSP/Source/StatisticsFunctions/StatisticsFunctions.c" 
		"${lib}/CMSIS/DSP/Source/SupportFunctions/SupportFunctions.c"   
		"${lib}/CMSIS/DSP/Source/SVMFunctions/SVMFunctions.c"   
		"${lib}/CMSIS/DSP/Source/TransformFunctions/TransformFunctions.c"   
    )
    list(APPEND inc_dirs "${lib}/CMSIS/DSP/Include")
    list(APPEND inc_dirs "${lib}/CMSIS/DSP/PrivateInclude")
endif()

# ------------------------------------ EPD ----------------------------------- #
if (${use_epd})
    list(APPEND src_dirs
        "${lib}/EPaper/Lib"
        "${lib}/EPaper/Examples"
        "${lib}/EPaper/Fonts"
        "${lib}/EPaper/GUI"
    )
    list(APPEND inc_dirs
        "${lib}/EPaper/Lib"
        "${lib}/EPaper/Examples"
        "${lib}/EPaper/Fonts"
        "${lib}/EPaper/GUI"
    )
endif()

# -------------------------------- Executable -------------------------------- #
foreach(src_dir ${src_dirs})
    file(GLOB src_files_tmp "${src_dir}/*.c" "${src_dir}/*.cpp" "${src_dir}/*.s")
    list(APPEND src_files ${src_files_tmp})
endforeach()
list(REMOVE_DUPLICATES src_files)

include_directories(${inc_dirs})

add_executable(${project_name}.elf ${src_files})
target_link_libraries(${project_name}.elf ${libs})

set(hex_file ${PROJECT_BINARY_DIR}/${project_name}.hex)
set(bin_file ${PROJECT_BINARY_DIR}/${project_name}.bin)

add_custom_command(TARGET ${project_name}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -I elf32-littlearm -Oihex $<TARGET_FILE:${project_name}.elf> ${hex_file}
    COMMAND ${CMAKE_OBJCOPY} -I elf32-littlearm -Obinary $<TARGET_FILE:${project_name}.elf> ${bin_file}
    COMMENT "Building ${hex_file} \nBuilding ${bin_file}"
)

# ---------------------------------- Clangd ---------------------------------- #
# Generate .clangd file for clangd language server
if (CMAKE_HOST_WIN32)
    execute_process(COMMAND where arm-none-eabi-gcc OUTPUT_VARIABLE compiler_path OUTPUT_STRIP_TRAILING_WHITESPACE)
    if (NOT compiler_path)
        message(FATAL_ERROR "arm-none-eabi-gcc not found")
    endif()
    # Note that clangd has a bug on Windows where it doesn't handle spaces in the path correctly
    # The best solution is to install the toolchain in a path without spaces. Using scoop is a good option.
    # If you have to use a path with spaces, the following code can convert the path to DOS 8.3 format as a workaround.
    if (compiler_path MATCHES " ")
        execute_process(COMMAND cmd /c for %A in ("${compiler_path}") do @echo %~sA OUTPUT_VARIABLE compiler_path OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif()
    string(REPLACE "\\" "\\\\" compiler_path ${compiler_path})
else()
    execute_process(COMMAND which arm-none-eabi-gcc OUTPUT_VARIABLE compiler_path OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

# detect if .clangd-template exists
if (EXISTS ${CMAKE_SOURCE_DIR}/.clangd-template)
    file(READ ${CMAKE_SOURCE_DIR}/.clangd-template clangd_template)
    string(REPLACE "%COMPILERPATH%" "${compiler_path}" clangd_template ${clangd_template})
    file(WRITE ${CMAKE_SOURCE_DIR}/.clangd ${clangd_template})
else()
    file(WRITE ${CMAKE_SOURCE_DIR}/.clangd "CompileFlags:\n  Compiler: ${compiler_path}\n")
endif()

# ----------------------------------- Flash ---------------------------------- #
if (${flash_program} MATCHES "jlink")
    file(WRITE "${CMAKE_SOURCE_DIR}/jlink-flash" "erase\nloadfile ${hex_file} 0 noreset\nreset\nexit\n")
    add_custom_target(flash
        COMMAND JLinkExe -device ${mcu_upper} -if swd -speed 4000 -JLinkScriptFile "${CMAKE_SOURCE_DIR}/Libs/Misc/jlink-script" -CommanderScript "${CMAKE_SOURCE_DIR}/jlink-flash"
        DEPENDS ${project_name}.elf
        COMMENT "Flashing ${hex_file}"
    )
elseif (${flash_program} MATCHES "pyocd")
    # file(WRITE "${CMAKE_SOURCE_DIR}/pyocd.yaml" "pack: [\"${CMAKE_SOURCE_DIR}/Misc/Puya.PY32F0xx_DFP.1.1.7.pack\"]")
    add_custom_target(Flash
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/Libs
        COMMAND pyocd erase -t ${mcu_lower} --chip --config "Misc/pyocd.yaml"
        COMMAND pyocd load ${hex_file} -t ${mcu_lower} --config "Misc/pyocd.yaml"
        DEPENDS ${project_name}.elf
        COMMENT "Flashing ${hex_file}"
    )
else()
    message(WARNING "Flash program not supported")
endif()