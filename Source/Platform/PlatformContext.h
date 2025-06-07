/*
 * PlatformContext.h - macros for identifying the platform context
 * Copyright (c) 2024 Johannes Przybilla. All Rights Reserved.
 */

#pragma once

//------------------------------------------------------------------------------------------------------------------------------------
// Compiler macros
//------------------------------------------------------------------------------------------------------------------------------------

#if defined(__clang__)
#  define ZV_COMPILER_CLANG 1
#elif defined(_MSC_VER)
#  define ZV_COMPILER_CL 1
#elif defined(__GNUC__)
#  define ZV_COMPILER_GCC 1
#else
#  error missing compiler detection
#endif

#if !defined(ZV_COMPILER_CLANG)
#  define ZV_COMPILER_CLANG 0
#endif
#if !defined(ZV_COMPILER_CL)
#  define ZV_COMPILER_CL 0
#endif
#if !defined(ZV_COMPILER_GCC)
#  define ZV_COMPILER_GCC 0
#endif

//------------------------------------------------------------------------------------------------------------------------------------
// File name macro
//------------------------------------------------------------------------------------------------------------------------------------

#if COMPILER_CLANG
#  define ZV_FILE_NAME __FILE_NAME__
#else
#  define ZV_FILE_NAME __FILE__
#endif

//------------------------------------------------------------------------------------------------------------------------------------
// OS macros
//------------------------------------------------------------------------------------------------------------------------------------

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#  define ZV_OS_WINDOWS 1
#elif defined(__gnu_linux__)
#  define ZV_OS_LINUX 1
#elif defined(__APPLE__) && defined(__MACH__)
#  define ZV_OS_MAC 1
#else
#  error missing OS detection
#endif

#if !defined(ZV_OS_WINDOWS)
#  define ZV_OS_WINDOWS 0
#endif
#if !defined(ZV_OS_LINUX)
#  define ZV_OS_LINUX 0
#endif
#if !defined(ZV_OS_MAC)
#  define ZV_OS_MAC 0
#endif

//------------------------------------------------------------------------------------------------------------------------------------
// Architecture macros
//------------------------------------------------------------------------------------------------------------------------------------

#if defined(__x86_64__) || defined(_M_X64) || defined(__amd64__) || defined(_M_AMD64)
#  define ZV_ARCH_X64 1
#elif defined(__i386__) || defined(_M_I86) || defined(i386) || defined(__i386) || defined(_M_IX86)
#  define ZV_ARCH_X86 1
#elif defined(__aarch64__) || defined(_M_ARM64)
#  define ZV_ARCH_ARM64 1
#elif defined(__arm__) || defined(_M_ARM)
#  define ZV_ARCH_ARM 1
#else
#  error missing arch detection
#endif

#if !defined(ZV_ARCH_X64)
#  define ZV_ARCH_X64 0
#endif
#if !defined(ZV_ARCH_X86)
#  define ZV_ARCH_X86 0
#endif
#if !defined(ZV_ARCH_ARM64)
#  define ZV_ARCH_ARM64 0
#endif
#if !defined(ZV_ARCH_ARM)
#  define ZV_ARCH_ARM 0
#endif