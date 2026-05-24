#!/usr/bin/env perl
# =============================================================================
# E-comOS Kernel Build System (Perl Version)
# Copyright (C) 2025,2026 Saladin5101
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
# =============================================================================

# Core Perl modules for timing, file operations, directory management,
# and process control
use strict;
use warnings;
use Time::HiRes qw(gettimeofday tv_interval);
use File::Path qw(make_path remove_tree);
use File::Copy;
use File::Basename;
use Cwd qw(abs_path);
use IPC::Open3;
use Symbol 'gensym';
use POSIX qw(:sys_wait_h);

# ===================== Configuration Section =====================
my $config = {
    # Toolchain executables for the build system
    tools => {
        cc      => 'gcc',              # C compiler
        as      => 'as',               # GNU assembler
        ld      => 'ld',               # GNU linker
        nasm    => 'nasm',             # Netwide Assembler
        objcopy => 'objcopy',          # Object file utility
        qemu    => 'qemu-system-x86_64', # x86_64 emulator
    },
    
    # Cross-compilation toolchain for bare-metal kernel development
    # Prefixed toolchain avoids host system library dependencies
    cross_tools => {
        cc      => 'x86_64-elf-gcc',   # Cross-compiler for x86_64 ELF
        as      => 'x86_64-elf-as',    # Cross-assembler
        ld      => 'x86_64-elf-ld',    # Cross-linker
        objcopy => 'x86_64-elf-objcopy', # Cross-objcopy
    },
    
    # Compiler flags for freestanding 64-bit kernel environment
    # These flags ensure no host dependencies and proper kernel ABI
    cflags => [
        '-m64',                        # 64-bit code generation
        '-ffreestanding',              # No standard library dependencies
        '-fno-builtin',                # Disable GCC built-in functions
        '-fno-stack-protector',        # No stack protection for kernel mode
        '-nostdlib',                   # Do not link standard libraries
        '-Wall',                       # Enable all warnings
        '-Wextra',                     # Enable extra warnings
        '-c',                          # Compile only, do not link
        '-mcmodel=large',              # Large code model for kernel
        '-Werror',                     # Treat warnings as errors
        '-pedantic',                   # Strict ISO C compliance
	'-g',                          # To debug
        '-Iinclude',                   # Include search directory
    ],
    
    # Assembler flags for 64-bit assembly code
    asflags => ['--64'],               # 64-bit mode for GNU assembler
    
    # NASM flags for different output formats
    nasm_elf_flags => ['-f', 'elf64'],  # Output ELF64 object files
    nasm_bin_flags => ['-f', 'bin'],    # Output raw binary files
    
    # Linker flags for kernel ELF executable generation
    ldflags => [
        '-m', 'elf_x86_64',           # Target x86_64 ELF format
        '-nostdlib',                  # No standard libraries
        '-T', 'multiboot2.ld',        # Custom linker script
    ],
    
    # UEFI-specific configuration for building UEFI applications
    efi => {
        inc      => '/usr/include/efi',      # EFI header directory
        inc_x64  => '/usr/include/efi/x86_64', # 64-bit specific headers
        lib      => '/usr/lib',              # Library directory
        lds      => '/usr/lib/elf_x86_64_efi.lds', # EFI linker script
        crt0     => '/usr/lib/crt0-efi-x86_64.o',  # EFI startup object
    },
    
    # Output file naming configuration
    output => {
        kernel_bin  => 'kernel.bin',     # Raw kernel binary output
        kernel_elf  => 'kernel.elf',     # ELF executable output
        kernel_efi  => 'kernel.efi',     # UEFI application output
        image_file  => 'canuse.img',     # Bootable disk image
        stage1_bin  => 'dos25-release.bin', # Stage 1 bootloader binary
        stage2_bin  => 'dos25-stage2.bin',  # Stage 2 bootloader binary
    },
    
    # Source file organization and categorization
    sources => {
        # Kernel C source files - core kernel components
        kernel => [
            'src/kernel/main.c',
            'src/kernel/syscall.c',
            'src/kernel/debug.c',
            'src/ipc/ipc.c',
            'src/mm/mm.c',
            'src/sched/sched.c',
            'src/printkit/print.c',
            'src/time/time.c',
            'src/user_space/user_mode.c',
            'klibc/libstring/string.c',
            'arch/x86_64/cpu/gdt.c',
            'arch/x86_64/interrupts/idt.c',
            'arch/x86_64/interrupts/isr.c',
            'arch/x86_64/interrupts/irq.c',
            'arch/x86_64/cpu/syscall.c',  # SYSCALL/SYSRET mechanism
	],
        
        # Assembly source files for low-level operations
        asm => [
            'arch/x86_64/cpu/context_switch.s',
            'boot/multiboot2_start.s',
            'boot/long_mode_switch.s',
            'src/arch/x86_64/switch_to_user_mode.S',
            'arch/x86_64/cpu/syscall_entry.S',  # SYSCALL entry point
        ],
        
        # Additional assembly objects referenced in build
        # These are generated from corresponding .s files
        asm_extra => [
            'arch/x86_64/interrupts/isr_asm.o',  # ISR assembly entry points
            'arch/x86_64/interrupts/irq_asm.o',  # IRQ assembly entry points
        ],
        
        # Bootloader source files for legacy BIOS boot
        stage1 => 'src/boot/bootsect.s',    # Stage 1 boot sector (512 bytes)
        stage2 => 'src/boot/bootsect-2nd.s',# Stage 2 bootloader extension
        
        # UEFI assembly startup code
        efi_asm => [
            'src/efi/efi_start.s',         # UEFI startup assembly code
        ],
    },
    
    # Build directory structure
    build_dir => 'build',     # Main build output directory
    obj_dir   => 'build/obj', # Object file directory
};

# ===================== Helper Functions =====================

# Print a formatted table header for build process output
# Displays columns for object file, command, execution time, and status
sub print_table_header {
    print "\n" . "=" x 120 . "\n";
    printf "%-40s | %-50s | %-12s | %-15s\n", 
           "OBJECT", "COMMAND", "TIME (s)", "STATUS";
    print "-" x 120 . "\n";
}

# Print a formatted table row with build information
# Truncates long strings for clean display in fixed-width columns
sub print_table_row {
    my ($object, $command, $time, $status) = @_;
    
    # Truncate long file paths and commands for display
    $object  = substr($object, 0, 37) . "..." if length($object) > 40;
    $command = substr($command, 0, 47) . "..." if length($command) > 50;
    
    printf "%-40s | %-50s | %12.3f | %-15s\n", 
           $object, $command, $time, $status;
}

# Execute a shell command and capture output, timing, and exit status
# Returns a hash reference with execution results
sub execute_command {
    my ($cmd, $desc) = @_;
    my $start_time = [gettimeofday];  # High-resolution timing start
    
    print "[EXEC] $desc\n" if $desc;
    print "      Command: $cmd\n" if $cmd;
    
    my ($output, $exit_code) = run_command($cmd);
    my $elapsed = tv_interval($start_time);  # Calculate execution time
    
    return {
        success   => ($exit_code == 0),  # True if command succeeded
        output    => $output,            # Command stdout/stderr
        time      => $elapsed,           # Execution time in seconds
        exit_code => $exit_code,         # Command exit code
    };
}

# Execute a command and capture its output
# Uses pipe to read both stdout and stderr
sub run_command {
    my ($cmd) = @_;
    
    my $pid = open(my $read, "$cmd 2>&1 |") or return ("Failed to execute: $!", 1);
    my $output = do { local $/; <$read> };  # Slurp all output
    close $read;
    
    my $exit_code = $? >> 8;  # Extract exit code from $?
    return ($output, $exit_code);
}

# Find a tool in the system, preferring cross-compiler if requested
# Searches cross_tools first, then falls back to standard tools
# Dies with error message if tool not found
sub find_tool {
    my ($tool, $cross) = @_;
    
    if ($cross) {
        my $cross_tool = $config->{cross_tools}{$tool};
        if (system("which $cross_tool >/dev/null 2>&1") == 0) {
            return $cross_tool;
        }
    }
    
    my $standard_tool = $config->{tools}{$tool};
    if (system("which $standard_tool >/dev/null 2>&1") == 0) {
        return $standard_tool;
    }
    
    die "ERROR: Cannot find tool '$tool'. Please install it.\n";
}

# Ensure a directory exists, create it if necessary
# Uses File::Path::make_path for recursive directory creation
sub ensure_dir {
    my ($dir) = @_;
    make_path($dir) unless -d $dir;
}

# ===================== Build Functions =====================

# Compile a C source file to an object file
# Converts source path to object path and applies compiler flags
sub compile_c_file {
    my ($source_file) = @_;
    
    # Transform source path to object path:
    # src/kernel/main.c -> build/obj/kernel/main.o
    my $obj_file = $source_file =~ s/\.c$/.o/r =~ s/^src/$config->{obj_dir}/r;
    
    ensure_dir(dirname($obj_file));  # Create output directory if needed
    
    my $cc = find_tool('cc', 1);  # Find compiler, prefer cross-compiler
    my @cflags = @{$config->{cflags}};
    
    # Build compiler command line
    my $cmd = join(' ', $cc, @cflags, '-o', $obj_file, $source_file);
    
    my $result = execute_command($cmd, "Compiling C file: $source_file");
    
    # Print result in build table
    print_table_row(
        $source_file,
        $cmd,
        $result->{time},
        $result->{success} ? "✓ SUCCESS" : "✗ FAILED"
    );
    
    # Print error output if compilation failed
    if (!$result->{success}) {
        print "ERROR OUTPUT:\n" . $result->{output} . "\n";
    }
    
    return {
        success  => $result->{success},
        obj_file => $obj_file,
        output   => $result->{output},
    };
}

# Compile an assembly source file to an object file
# Uses GNU assembler with architecture-specific flags
sub compile_asm_file {
    my ($source_file) = @_;
    
    # Transform source path to object path:
    # arch/x86_64/cpu/context_switch.s -> build/obj/x86_64/cpu/context_switch.o
    # src/arch/x86_64/switch_to_user_mode.S -> build/obj/arch/x86_64/switch_to_user_mode.o
    my $obj_file = $source_file =~ s/\.[sS]$/.o/r =~ s/^arch/$config->{obj_dir}/r =~ s/^src/$config->{obj_dir}/r;
    
    ensure_dir(dirname($obj_file));
    
    my $as = find_tool('as', 1);  # Find assembler, prefer cross-assembler
    my @asflags = @{$config->{asflags}};
    
    my $cmd = join(' ', $as, @asflags, '-o', $obj_file, $source_file);
    
    my $result = execute_command($cmd, "Assembling: $source_file");
    
    print_table_row(
        $source_file,
        $cmd,
        $result->{time},
        $result->{success} ? "✓ SUCCESS" : "✗ FAILED"
    );
    
    if (!$result->{success}) {
        print "ERROR OUTPUT:\n" . $result->{output} . "\n";
    }
    
    return {
        success  => $result->{success},
        obj_file => $obj_file,
        output   => $result->{output},
    };
}

# Assemble NASM source file to either ELF object or raw binary
# $is_binary determines output format (ELF vs raw binary)
sub compile_nasm_file {
    my ($source_file, $is_binary) = @_;
    my $output_file = $source_file =~ s/\.s$/.bin/r;
    
    ensure_dir(dirname($output_file));
    
    my $nasm = $config->{tools}{nasm};
    my @flags = $is_binary ? @{$config->{nasm_bin_flags}} : @{$config->{nasm_elf_flags}};
    
    my $cmd = join(' ', $nasm, @flags, $source_file, '-o', $output_file);
    
    my $result = execute_command($cmd, "NASM assembling: $source_file");
    
    print_table_row(
        $source_file,
        $cmd,
        $result->{time},
        $result->{success} ? "✓ SUCCESS" : "✗ FAILED"
    );
    
    if (!$result->{success}) {
        print "ERROR OUTPUT:\n" . $result->{output} . "\n";
    }
    
    return {
        success      => $result->{success},
        output_file  => $output_file,
        output       => $result->{output},
    };
}

# Link all object files into a kernel ELF executable
# Uses custom linker script and produces final kernel binary
sub link_kernel {
    my (@obj_files) = @_;
    my $output_file = $config->{output}{kernel_elf};
    
    # Reorder object files: put boot files first, then all other files
    # This ensures that the boot assembly code is linked first, which is required
    # for the Multiboot2 header to be at the beginning of the kernel image.
    my @boot_files = grep { m{boot/} } @obj_files;
    my @other_files = grep { !m{boot/} } @obj_files;
    
    # Reorder: boot files first, then other files
    @obj_files = (@boot_files, @other_files);
    
    # Debug output to verify ordering
    if (@boot_files) {
        print "[DEBUG] Boot files to be linked first:\n";
        foreach my $boot (@boot_files) {
            print "  - $boot\n";
        }
    }
    
    my $ld = find_tool('ld', 1);
    my @ldflags = @{$config->{ldflags}};
    
    my $cmd = join(' ', $ld, @ldflags, '-o', $output_file, @obj_files);
    
    my $result = execute_command($cmd, "Linking kernel");
    
    print_table_row(
        "LINK KERNEL",
        $cmd,
        $result->{time},
        $result->{success} ? "✓ LINKED" : "✗ FAILED"
    );
    
    if ($result->{success}) {
        my $size = -s $output_file;
        print "✓ Kernel ELF size: $size bytes\n";
    } else {
        print "ERROR OUTPUT:\n" . $result->{output} . "\n";
    }
    
    return {
        success      => $result->{success},
        output_file  => $output_file,
        output       => $result->{output},
    };
}

# Create a bootable disk image with bootloader and kernel
# Creates 1.44MB floppy image and writes components at specific sectors
sub create_boot_image {
    my ($stage1, $stage2, $kernel) = @_;
    my $image_file = $config->{output}{image_file};
    
    # Create 1.44MB floppy image (2880 sectors * 512 bytes = 1.44MB)
    my $cmd1 = "dd if=/dev/zero of=$image_file bs=512 count=2880 2>/dev/null";
    my $cmd2 = "dd if=$stage1 of=$image_file bs=512 count=1 conv=notrunc 2>/dev/null";      # Boot sector at sector 0
    my $cmd3 = "dd if=$stage2 of=$image_file bs=512 seek=1 conv=notrunc 2>/dev/null";      # Stage2 at sector 1
    my $cmd4 = "dd if=$kernel of=$image_file bs=512 seek=14 conv=notrunc 2>/dev/null";     # Kernel at sector 14
    
    print "[INFO] Creating bootable image: $image_file\n";
    
    my $result1 = execute_command($cmd1, "Creating blank image");
    my $result2 = execute_command($cmd2, "Writing stage1 bootloader");
    my $result3 = execute_command($cmd3, "Writing stage2 bootloader");
    my $result4 = execute_command($cmd4, "Writing kernel");
    
    if ($result1->{success} && $result2->{success} && 
        $result3->{success} && $result4->{success}) {
        
        my $size = -s $image_file;
        print "✓ Bootable image created: $image_file ($size bytes)\n";
        return 1;
    }
    
    return 0;
}

# Run the kernel in QEMU emulator for testing
# Uses floppy disk image and provides monitor interface
sub run_qemu {
    my ($image_file) = @_;
    my $qemu = $config->{tools}{qemu};
    
    if (!-f $image_file) {
        print "ERROR: Image file not found: $image_file\n";
        return 0;
    }
    
    print "[INFO] Starting QEMU with image: $image_file\n";
    print "       Press Ctrl+A, X to exit QEMU\n\n";
    
    my $cmd = "$qemu -fda $image_file -monitor stdio";
    system($cmd);
    
    return 1;
}

# Clean all build artifacts and directories
# Removes object files, binaries, and build directories
sub clean_build {
    my @patterns = (
        '*.o', '*.bin', '*.elf', '*.efi', '*.img',
        '*.log', '*.txt',
        "$config->{obj_dir}/*",
        "$config->{build_dir}/*",
    );
    
    print "[INFO] Cleaning build artifacts...\n";
    
    # Delete files matching patterns
    foreach my $pattern (@patterns) {
        my @files = glob($pattern);
        foreach my $file (@files) {
            unlink $file if -f $file;
            print "  Deleted: $file\n";
        }
    }
    
    # Remove build directories
    remove_tree($config->{obj_dir}) if -d $config->{obj_dir};
    remove_tree($config->{build_dir}) if -d $config->{build_dir};
    
    print "✓ Clean complete\n";
    return 1;
}

# ===================== Main Build Process =====================

# Main build function: compile all sources and link kernel
# Coordinates the entire build process and reports results
sub build_all {
    my $start_time = [gettimeofday];  # Start timing the entire build
    
    print "=" x 60 . "\n";
    print "E-comOS Kernel Build System (Perl Version)\n";
    print "=" x 60 . "\n\n";
    
    print_table_header();  # Print build output table header
    
    my @obj_files;        # Accumulate successful object files
    my $all_success = 1;  # Track overall build success
    
    # Phase 1: Compile all C source files
    foreach my $c_file (@{$config->{sources}{kernel}}) {
        if (-f $c_file) {
            my $result = compile_c_file($c_file);
            push @obj_files, $result->{obj_file} if $result->{success};
            $all_success &&= $result->{success};  # Logical AND for success tracking
        } else {
            print "WARNING: Source file not found: $c_file\n";
        }
    }
    
    # Phase 2: Compile assembly source files
    foreach my $asm_file (@{$config->{sources}{asm}}) {
        if (-f $asm_file) {
            my $result = compile_asm_file($asm_file);
            push @obj_files, $result->{obj_file} if $result->{success};
            $all_success &&= $result->{success};
        } else {
            print "WARNING: Assembly file not found: $asm_file\n";
        }
    }
    
    # Phase 3: Compile extra assembly files (referenced by asm_extra list)
    # Handles ISR and IRQ assembly stubs
    foreach my $extra_asm (@{$config->{sources}{asm_extra}}) {
        if ($extra_asm =~ /isr_asm\.o$/) {
            my $source = 'arch/x86_64/interrupts/isr.s';
            if (-f $source) {
                my $result = compile_asm_file($source);
                push @obj_files, $result->{obj_file} if $result->{success};
                $all_success &&= $result->{success};
            }
        } elsif ($extra_asm =~ /irq_asm\.o$/) {
            my $source = 'arch/x86_64/interrupts/irq.s';
            if (-f $source) {
                my $result = compile_asm_file($source);
                push @obj_files, $result->{obj_file} if $result->{success};
                $all_success &&= $result->{success};
            }
        }
    }
    
    # Phase 4: Embed init.bin as a linkable object
    ensure_dir($config->{obj_dir});
    my $objcopy = find_tool('objcopy', 1);
    for my $payload (['payload/init.bin', "$config->{obj_dir}/init_bin.o"],
                     ['payload/ebts.bin', "$config->{obj_dir}/ebts_bin.o"]) {
        my ($bin, $obj) = @$payload;
        if (-f $bin && -s $bin) {
            my $embed_cmd = "$objcopy -I binary -O elf64-x86-64 -B i386:x86-64 $bin $obj";
            my $embed = execute_command($embed_cmd, "Embedding $bin");
            print_table_row("$bin -> $obj", $embed_cmd, $embed->{time},
                            $embed->{success} ? '✓ SUCCESS' : '✗ FAILED');
            if ($embed->{success}) {
                push @obj_files, $obj;
            } else {
                print "ERROR OUTPUT:\n" . $embed->{output} . "\n";
                $all_success = 0;
            }
        } else {
            print "WARNING: $bin not found or empty — skipping\n";
        }
    }

    # Phase 5: Link all object files into kernel executable
    if ($all_success && @obj_files) {
        my $result = link_kernel(@obj_files);
        $all_success &&= $result->{success};
        
        if ($result->{success}) {
            # Create raw binary copy of the ELF kernel
            copy($result->{output_file}, $config->{output}{kernel_bin});
            print "✓ Kernel binary created: $config->{output}{kernel_bin}\n";
        }
    } else {
        print "ERROR: Cannot link kernel - no object files or compilation failed\n";
        $all_success = 0;
    }
    
    # Calculate and print total build time
    my $total_time = tv_interval($start_time);
    print "\n" . "=" x 60 . "\n";
    print "BUILD COMPLETE\n";
    print "Total time: $total_time seconds\n";
    print "Status: " . ($all_success ? "✓ SUCCESS" : "✗ FAILED") . "\n";
    print "=" x 60 . "\n";
    
    return $all_success;
}

# Create bootable disk image with optional bootloader
# Builds bootloader if source files exist, otherwise creates minimal image
sub build_image {
    my $start_time = [gettimeofday];
    
    print "=" x 60 . "\n";
    print "Creating Bootable Image\n";
    print "=" x 60 . "\n\n";
    
    # Ensure kernel is built before creating image
    if (!-f $config->{output}{kernel_bin}) {
        print "Kernel not found. Building kernel first...\n";
        if (!build_all()) {
            print "ERROR: Cannot build kernel. Aborting image creation.\n";
            return 0;
        }
    }
    
    # Check for bootloader source files
    my $stage1 = $config->{sources}{stage1};
    my $stage2 = $config->{sources}{stage2};
    
    if (!-f $stage1 || !-f $stage2) {
        # Bootloader not found, create minimal kernel-only image
        print "WARNING: Bootloader source files not found:\n";
        print "  Stage1: $stage1\n" if !-f $stage1;
        print "  Stage2: $stage2\n" if !-f $stage2;
        print "\nCreating minimal image with kernel only...\n";
        
        my $image_file = $config->{output}{image_file};
        my $kernel = $config->{output}{kernel_bin};
        
        # Create simple image with kernel at the beginning
        my $cmd = "dd if=/dev/zero of=$image_file bs=512 count=2880 2>/dev/null && " .
                  "dd if=$kernel of=$image_file bs=512 conv=notrunc 2>/dev/null";
        
        my $result = execute_command($cmd, "Creating minimal kernel image");
        
        if ($result->{success}) {
            my $size = -s $image_file;
            print "✓ Minimal image created: $image_file ($size bytes)\n";
            print "  Note: This image may not be bootable without a bootloader\n";
            
            my $total_time = tv_interval($start_time);
            print "\nImage creation time: $total_time seconds\n";
            return 1;
        } else {
            print "ERROR: Failed to create image\n";
            return 0;
        }
    } else {
        # Build full bootloader and create complete bootable image
        print_table_header();
        
        my $stage1_result = compile_nasm_file($stage1, 1);  # Compile as binary
        my $stage2_result = compile_nasm_file($stage2, 1);  # Compile as binary
        
        if ($stage1_result->{success} && $stage2_result->{success}) {
            my $success = create_boot_image(
                $stage1_result->{output_file},
                $stage2_result->{output_file},
                $config->{output}{kernel_bin}
            );
            
            my $total_time = tv_interval($start_time);
            print "\nImage creation time: $total_time seconds\n";
            return $success;
        } else {
            print "ERROR: Failed to build bootloader\n";
            return 0;
        }
    }
}

# Build a bootable GRUB2 ISO image
sub build_iso {
    my $start_time = [gettimeofday];

    print "=" x 60 . "\n";
    print "Creating GRUB2 ISO\n";
    print "=" x 60 . "\n\n";

    my $kernel_elf = $config->{output}{kernel_elf};
    if (!-f $kernel_elf) {
        print "Kernel not found. Building kernel first...\n";
        if (!build_all()) {
            print "ERROR: Cannot build kernel. Aborting ISO creation.\n";
            return 0;
        }
    }

    # Copy fresh kernel.elf into iso tree
    File::Copy::copy($kernel_elf, 'iso/boot/kernel.elf')
        or die "Cannot copy $kernel_elf to iso/boot/kernel.elf: $!\n";

    my $iso_file = 'ecomos.iso';
    my $cmd = "grub2-mkrescue -o $iso_file iso";
    my $result = execute_command($cmd, "Creating GRUB2 ISO");

    print_table_header();
    print_table_row('ecomos.iso', $cmd, $result->{time},
                    $result->{success} ? '✓ SUCCESS' : '✗ FAILED');

    if ($result->{success}) {
        my $size = -s $iso_file;
        print "✓ ISO created: $iso_file ($size bytes)\n";
    } else {
        print "ERROR OUTPUT:\n" . $result->{output} . "\n";
    }

    my $total_time = tv_interval($start_time);
    print "\nISO creation time: $total_time seconds\n";
    return $result->{success};
}

# ===================== Main Entry Point =====================

# Main dispatcher function - handles command-line arguments
# Routes to appropriate build functions based on action
sub main {
    my $action = shift @ARGV || 'all';  # Default action is 'all'
    
    if ($action eq 'all' || $action eq 'build') {
        return build_all();
    }
    elsif ($action eq 'clean') {
        return clean_build();
    }
    elsif ($action eq 'image') {
        return build_image();
    }
    elsif ($action eq 'run') {
        my $image_file = $config->{output}{image_file};
        if (!-f $image_file) {
            print "Image not found. Building image first...\n";
            if (!build_image()) {
                print "ERROR: Cannot build image. Aborting run.\n";
                return 0;
            }
        }
        return run_qemu($image_file);
    }
    elsif ($action eq 'iso') {
        return build_iso();
    }
    elsif ($action eq 'help' || $action eq '--help' || $action eq '-h') {
        print_help();
        return 1;
    }
    else {
        print "ERROR: Unknown action: $action\n\n";
        print_help();
        return 0;
    }
}

# Print usage information and help message
sub print_help {
    print <<"HELP";
E-comOS Build System (Perl Version)

Usage: $0 [action]

Available actions:
  all/build    Build everything (default)
  clean        Clean all build artifacts
  image        Create bootable disk image
  run          Run in QEMU
  help         Show this help message

Examples:
  $0              # Build everything
  $0 clean        # Clean build files
  $0 image        # Create bootable image
  $0 run          # Run in QEMU

Configuration:
  Toolchain:    Uses cross-compiler if available, falls back to system tools
  C Flags:      @{$config->{cflags}}
  LD Flags:     @{$config->{ldflags}}

HELP
}

# Execute the build system with appropriate exit code
# Exit code 0 indicates success, 1 indicates failure
exit(main() ? 0 : 1);


