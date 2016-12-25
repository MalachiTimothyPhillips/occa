/* The MIT License (MIT)
 *
 * Copyright (c) 2014-2016 David Medina and Tim Warburton
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 */

#include "occa/defines.hpp"

#if   (OCCA_OS & OCCA_LINUX_OS)
#  include <unistd.h>
#  include <sys/time.h>
#  include <sys/sysctl.h>
#  include <sys/sysinfo.h>
#  include <dlfcn.h>
#  include <errno.h>
#elif (OCCA_OS & OCCA_OSX_OS)
#  include <mach/mach_host.h>
#  include <sys/sysctl.h>
#  include <dlfcn.h>
#  ifdef __clang__
#    include <CoreServices/CoreServices.h>
#    include <mach/mach_time.h>
#  else
#    include <mach/clock.h>
#    include <mach/mach.h>
#  endif
#else
#  ifndef NOMINMAX
#    define NOMINMAX // Clear min/max macros
#  endif
#  include <windows.h>
#endif

#include <sys/types.h>
#include <fcntl.h>

#include "occa/tools/env.hpp"
#include "occa/tools/hash.hpp"
#include "occa/tools/io.hpp"
#include "occa/tools/misc.hpp"
#include "occa/tools/string.hpp"
#include "occa/tools/sys.hpp"
#include "occa/parser/tools.hpp"

namespace occa {
  namespace flags {
    const int checkCacheDir = (1 << 0);
  }

  namespace sys {
    double currentTime() {
#if (OCCA_OS & OCCA_LINUX_OS)

      timespec ct;
      clock_gettime(CLOCK_MONOTONIC, &ct);

      return (double) (ct.tv_sec + (1.0e-9 * ct.tv_nsec));

#elif (OCCA_OS == OCCA_OSX_OS)
#  ifdef __clang__
      uint64_t ct;
      ct = mach_absolute_time();

      const Nanoseconds ct2 = AbsoluteToNanoseconds(*(AbsoluteTime *) &ct);

      return ((double) 1.0e-9) * ((double) ( *((uint64_t*) &ct2) ));
#  else
      clock_serv_t cclock;
      host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);

      mach_timespec_t ct;
      clock_get_time(cclock, &ct);

      mach_port_deallocate(mach_task_self(), cclock);

      return (double) (ct.tv_sec + (1.0e-9 * ct.tv_nsec));
#  endif
#elif (OCCA_OS == OCCA_WINDOWS_OS)
      static LARGE_INTEGER freq;
      static bool haveFreq = false;

      if (!haveFreq) {
        QueryPerformanceFrequency(&freq);
        haveFreq=true;
      }

      LARGE_INTEGER ct;

      QueryPerformanceCounter(&ct);

      return ((double) (ct.QuadPart)) / ((double) (freq.QuadPart));
#endif
    }

    //---[ System Calls ]---------------
    int call(const std::string &cmdline) {
#if (OCCA_OS & (OCCA_LINUX_OS | OCCA_OSX_OS))
      FILE *fp = popen(cmdline.c_str(), "r");
      return pclose(fp);
#else
      FILE *fp = _popen(cmdline.c_str(), "r");
      return _pclose(fp);
#endif
    }

    int call(const std::string &cmdline, std::string &output) {
#if (OCCA_OS & (OCCA_LINUX_OS | OCCA_OSX_OS))
      FILE *fp = popen(cmdline.c_str(), "r");
#else
      FILE *fp = _popen(cmdline.c_str(), "r");
#endif

      size_t lineBytes = 512;
      char lineBuffer[512];

      while(fgets(lineBuffer, lineBytes, fp))
        output += lineBuffer;

#if (OCCA_OS & (OCCA_LINUX_OS | OCCA_OSX_OS))
      return pclose(fp);
#else
      return _pclose(fp);
#endif
    }

    std::string expandEnvVariables(const std::string &str) {
      const char *c = str.c_str();
      const char *c0 = c;
      std::string expstr;

      while (*c != '\0') {
#if (OCCA_OS & (OCCA_LINUX_OS | OCCA_OSX_OS))
        if ((*c == '$') && ((c0 < c) || (*(c - 1) != '\\'))) {
          if (*(c + 1) == '{') {
            const char *cStart = c + 2;
            skipTo(c, '}');

            if (*c == '\0')
              return expstr;

            expstr += env::var(std::string(cStart, c - cStart));
          } else {
            const char *cStart = c + 1;
            skipTo(c, '/');
            expstr += env::var(std::string(cStart, c - cStart));
          }
        }
#else
        if (*c == '%') {
          const char *cStart = (++c);
          skipTo(c, '%');
          expstr += env::var(std::string(cStart, c - cStart));
        }
#endif
        else {
          expstr += *c;
        }
        ++c;
      }

      return expstr;
    }

    void rmdir(const std::string &dir) {
#if (OCCA_OS & (OCCA_LINUX_OS | OCCA_OSX_OS))
      ::rmdir(dir.c_str());
#else
      ::_rmdir(dir.c_str());
#endif
    }

    int mkdir(const std::string &dir) {
      errno = 0;

#if (OCCA_OS & (OCCA_LINUX_OS | OCCA_OSX_OS))
      return ::mkdir(dir.c_str(), 0755);
#else
      return ::_mkdir(dir.c_str());
#endif
    }

    void mkpath(const std::string &dir) {
      strVector_t path = split(io::filename(dir), '/');

      const int dirCount = (int) path.size();
      std::string sPath;
      int makeFrom = -1;

#if (OCCA_OS & (OCCA_LINUX_OS | OCCA_OSX_OS))
      const int firstDir = 0;
      if (dirCount == 0)
        return;
      sPath += '/';
#else
      const int firstDir = 1;
      if (dirCount <= 1)
        return;
      sPath += path[0];
      sPath += '/';
#endif

      for (int d = firstDir; d < dirCount; ++d) {
        sPath += path[d];
        if (!dirExists(sPath)) {
          makeFrom = d;
          break;
        }
        sPath += '/';
      }

      if (0 < makeFrom) {
        sys::mkdir(sPath);

        for (int d = (makeFrom + 1); d < dirCount; ++d) {
          sPath += '/';
          sPath += path[d];

          sys::mkdir(sPath);
        }
      }
    }

    bool dirExists(const std::string &dir_) {
      std::string dir = expandEnvVariables(dir_);
      strip(dir);

      struct stat statInfo;
      return ((stat(dir.c_str(), &statInfo) == 0) &&
              (statInfo.st_mode &S_IFDIR));
    }

    bool fileExists(const std::string &filename_,
                    const int flags) {

      std::string filename = expandEnvVariables(filename_);
      strip(filename);

      if (flags & flags::checkCacheDir)
        return fileExists(io::filename(filename));

      struct stat statInfo;

      return (stat(filename.c_str(), &statInfo) == 0);
    }

    //---[ Processor Info ]-------------
    std::string getFieldFrom(const std::string &command,
                             const std::string &field) {
#if (OCCA_OS & LINUX)
      std::string shellToolsFile = io::filename("occa://occa/scripts/shellTools.sh");

      if (!sys::fileExists(shellToolsFile)) {
        sys::mkpath(dirname(shellToolsFile));

        std::ofstream fs2;
        fs2.open(shellToolsFile.c_str());

        fs2 << getCachedScript("shellTools.sh");

        fs2.close();
      }

      std::stringstream ss;

      ss << "echo \"(. " << shellToolsFile << "; " << command << " '" << field << "')\" | bash";

      std::string sCommand = ss.str();

      FILE *fp;
      fp = popen(sCommand.c_str(), "r");

      const int bufferSize = 4096;
      char *buffer = new char[bufferSize];

      ignoreResult( fread(buffer, sizeof(char), bufferSize, fp) );

      pclose(fp);

      int end;

      for (end = 0; end < bufferSize; ++end) {
        if (buffer[end] == '\n')
          break;
      }

      std::string ret(buffer, end);

      delete [] buffer;

      return ret;
#else
      return "";
#endif
    }

    std::string getProcessorName() {
#if   (OCCA_OS & OCCA_LINUX_OS)
      return getFieldFrom("getCPUINFOField", "model name");
#elif (OCCA_OS == OCCA_OSX_OS)
      size_t bufferSize = 100;
      char buffer[100];

      sysctlbyname("machdep.cpu.brand_string",
                   &buffer, &bufferSize,
                   NULL, 0);

      return std::string(buffer);
#elif (OCCA_OS == OCCA_WINDOWS_OS)
      char buffer[MAX_COMPUTERNAME_LENGTH + 1];
      int bytes;

      GetComputerName((LPSTR) buffer, (LPDWORD) &bytes);

      return std::string(buffer, bytes);
#endif
    }

    int getCoreCount() {
#if (OCCA_OS & (OCCA_LINUX_OS | OCCA_OSX_OS))
      return sysconf(_SC_NPROCESSORS_ONLN);
#elif (OCCA_OS == OCCA_WINDOWS_OS)
      SYSTEM_INFO sysinfo;
      GetSystemInfo(&sysinfo);
      return sysinfo.dwNumberOfProcessors;
#endif
    }

    int getProcessorFrequency() {
#if   (OCCA_OS & OCCA_LINUX_OS)
      std::stringstream ss;
      int freq;

      ss << getFieldFrom("getCPUINFOField", "cpu MHz");

      ss >> freq;

      return freq;
#elif (OCCA_OS == OCCA_OSX_OS)
      uint64_t frequency = 0;
      size_t size = sizeof(frequency);

      int error = sysctlbyname("hw.cpufrequency", &frequency, &size, NULL, 0);

      OCCA_CHECK(error != ENOMEM,
                 "Error getting CPU Frequency.\n");

      return frequency/1.0e6;
#elif (OCCA_OS == OCCA_WINDOWS_OS)
      LARGE_INTEGER performanceFrequency;
      QueryPerformanceFrequency(&performanceFrequency);

      return (int) (((double) performanceFrequency.QuadPart) / 1000.0);
#endif
    }

    std::string getProcessorCacheSize(int level) {
#if   (OCCA_OS & OCCA_LINUX_OS)
      std::stringstream field;

      field << 'L' << level;

      if (level == 1)
        field << 'd';

      field << " cache";

      return getFieldFrom("getLSCPUField", field.str());
#elif (OCCA_OS == OCCA_OSX_OS)
      std::stringstream ss;
      ss << "hw.l" << level;

      if (level == 1)
        ss << 'd';

      ss << "cachesize";

      std::string field = ss.str();

      uint64_t cache = 0;
      size_t size = sizeof(cache);

      int error = sysctlbyname(field.c_str(), &cache, &size, NULL, 0);

      OCCA_CHECK(error != ENOMEM,
                 "Error getting L" << level << " Cache Size.\n");

      return stringifyBytes(cache);
#elif (OCCA_OS == OCCA_WINDOWS_OS)
      std::stringstream ss;
      DWORD cache = 0;
      int bytes = 0;

      PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = NULL;

      GetLogicalProcessorInformation(buffer, (LPDWORD) &bytes);

      OCCA_CHECK((GetLastError() == ERROR_INSUFFICIENT_BUFFER),
                 "[GetLogicalProcessorInformation] Failed");

      buffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION) sys::malloc(bytes);

      bool passed = GetLogicalProcessorInformation(buffer, (LPDWORD) &bytes);

      OCCA_CHECK(passed,
                 "[GetLogicalProcessorInformation] Failed");

      PSYSTEM_LOGICAL_PROCESSOR_INFORMATION pos = buffer;
      int off = 0;
      int sk = sizeof(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION);

      while ((off + sk) <= bytes) {
        switch(pos->Relationship) {
        case RelationCache:{
          CACHE_DESCRIPTOR info = pos->Cache;

          if (info.Level == level) {
            cache = info.Size;
            break;
          }
        }
        }
        ++pos;
        off += sk;
      }

      sys::free(buffer);

      return stringifyBytes(cache);
#endif
    }

    udim_t installedRAM() {
#if   (OCCA_OS & OCCA_LINUX_OS)
      struct sysinfo info;

      const int error = sysinfo(&info);

      if (error != 0)
        return 0;

      return info.totalram;
#elif (OCCA_OS == OCCA_OSX_OS)
      int64_t ram;

      int mib[2]   = {CTL_HW, HW_MEMSIZE};
      size_t bytes = sizeof(ram);

      sysctl(mib, 2, &ram, &bytes, NULL, 0);

      return ram;
#elif (OCCA_OS == OCCA_WINDOWS_OS)
      return 0;
#endif
    }

    udim_t availableRAM() {
#if   (OCCA_OS & OCCA_LINUX_OS)
      struct sysinfo info;

      const int error = sysinfo(&info);

      if (error != 0)
        return 0;

      return info.freeram;
#elif (OCCA_OS == OCCA_OSX_OS)
      mach_msg_type_number_t infoCount = HOST_VM_INFO_COUNT;
      mach_port_t hostPort = mach_host_self();

      vm_statistics_data_t hostInfo;
      kern_return_t status;
      vm_size_t pageSize;

      status = host_page_size(hostPort, &pageSize);

      if (status != KERN_SUCCESS)
        return 0;

      status = host_statistics(hostPort,
                               HOST_VM_INFO,
                               (host_info_t) &hostInfo,
                               &infoCount);

      if (status != KERN_SUCCESS)
        return 0;

      return (hostInfo.free_count * pageSize);
#elif (OCCA_OS == OCCA_WINDOWS_OS)
      return 0;
#endif
    }

    int compilerVendor(const std::string &compiler) {
#if (OCCA_OS & (OCCA_LINUX_OS | OCCA_OSX_OS))
      const std::string safeCompiler = io::removeSlashes(compiler);
      int vendor_ = sys::vendor::notFound;
      std::stringstream ss;

      const std::string compilerVendorTest = env::OCCA_DIR + "/scripts/compilerVendorTest.cpp";
      hash_t hash = occa::hashFile(compilerVendorTest);
      hash ^= occa::hash(vendor_);
      hash ^= occa::hash(compiler);

      const std::string srcFilename = io::cacheFile(compilerVendorTest, "compilerVendorTest.cpp", hash);
      const std::string binaryFilename = io::dirname(srcFilename) + "binary";
      const std::string outFilename = io::dirname(srcFilename) + "output";

      const std::string hashTag = "compiler";
      if (!io::haveHash(hash, hashTag)) {
        io::waitForHash(hash, hashTag);
      } else {
        if (!sys::fileExists(outFilename)) {
          ss << compiler
             << ' '    << srcFilename
             << " -o " << binaryFilename
             << " > /dev/null 2>&1";

          const int compileError = system(ss.str().c_str());

          if (!compileError) {
            int exitStatus = system(binaryFilename.c_str());
            int vendorBit  = WEXITSTATUS(exitStatus);

            if (vendorBit < sys::vendor::b_max)
              vendor_ = (1 << vendorBit);
          }

          ss.str("");
          ss << vendor_;

          io::write(outFilename, ss.str());
          io::releaseHash(hash, hashTag);

          return vendor_;
        }
        io::releaseHash(hash, hashTag);
      }

      ss << io::read(outFilename);
      ss >> vendor_;

      return vendor_;

#elif (OCCA_OS == OCCA_WINDOWS_OS)
#  if OCCA_USING_VS
      return sys::vendor::VisualStudio;
#  endif

      if (compiler.find("cl.exe") != std::string::npos) {
        return sys::vendor::VisualStudio;
      }
#endif
    }

    std::string compilerSharedBinaryFlags(const std::string &compiler) {
      return compilerSharedBinaryFlags( sys::compilerVendor(compiler) );
    }

    std::string compilerSharedBinaryFlags(const int vendor_) {
      if (vendor_ & (sys::vendor::GNU   |
                     sys::vendor::LLVM  |
                     sys::vendor::Intel |
                     sys::vendor::IBM   |
                     sys::vendor::PGI   |
                     sys::vendor::Cray  |
                     sys::vendor::Pathscale)) {

        return "-x c++ -fPIC -shared"; // [-] -x c++ for now
      } else if (vendor_ & sys::vendor::HP) {
        return "+z -b";
      } else if (vendor_ & sys::vendor::VisualStudio) {
#if OCCA_DEBUG_ENABLED
        return "/TP /LD /MDd";
#else
        return "/TP /LD /MD";
#endif
      }

      return "";
    }

    void addSharedBinaryFlagsTo(const std::string &compiler, std::string &flags) {
      addSharedBinaryFlagsTo(sys::compilerVendor(compiler), flags);
    }

    void addSharedBinaryFlagsTo(const int vendor_, std::string &flags) {
      std::string sFlags = sys::compilerSharedBinaryFlags(vendor_);

      if (flags.size() == 0)
        flags = sFlags;

      if (flags.find(sFlags) == std::string::npos)
        flags = (sFlags + " " + flags);
    }

    //---[ Dynamic Methods ]------------
    void* malloc(udim_t bytes) {
      void* ptr;

#if   (OCCA_OS & (OCCA_LINUX_OS | OCCA_OSX_OS))
      ignoreResult( posix_memalign(&ptr, env::OCCA_MEM_BYTE_ALIGN, bytes) );
#elif (OCCA_OS == OCCA_WINDOWS_OS)
      ptr = ::malloc(bytes);
#endif

      return ptr;
    }

    void free(void *ptr) {
      ::free(ptr);
    }

    void* dlopen(const std::string &filename,
                 const hash_t &hash,
                 const std::string &hashTag) {

#if (OCCA_OS & (OCCA_LINUX_OS | OCCA_OSX_OS))
      void *dlHandle = ::dlopen(filename.c_str(), RTLD_NOW);

      if ((dlHandle == NULL) && hash.initialized) {
        io::releaseHash(hash, hashTag);

        OCCA_CHECK(false,
                   "Error loading binary [" << io::shortname(filename) << "] with dlopen");
      }
#else
      void *dlHandle = LoadLibraryA(filename.c_str());

      if ((dlHandle == NULL) && hash.initialized) {
        io::releaseHash(hash, hashTag);

        OCCA_CHECK(dlHandle != NULL,
                   "Error loading dll [" << io::shortname(filename) << "] (WIN32 error: " << GetLastError() << ")");
      }
#endif

      return dlHandle;
    }

    handleFunction_t dlsym(void *dlHandle,
                           const std::string &functionName,
                           const hash_t &hash,
                           const std::string &hashTag) {

#if (OCCA_OS & (OCCA_LINUX_OS | OCCA_OSX_OS))
      void *sym = ::dlsym(dlHandle, functionName.c_str());

      char *dlError;

      if (((dlError = dlerror()) != NULL) && hash.initialized) {
        io::releaseHash(hash, hashTag);

        OCCA_CHECK(false,
                   "Error loading symbol from binary with dlsym (DL Error: " << dlError << ")");
      }
#else
      void *sym = GetProcAddress((HMODULE) dlHandle, functionName.c_str());

      if ((sym == NULL) && hash.initialized) {

        OCCA_CHECK(false,
                   "Error loading symbol from binary with GetProcAddress");
      }
#endif

      handleFunction_t sym2;

      ::memcpy(&sym2, &sym, sizeof(sym));

      return sym2;
    }

    void dlclose(void *dlHandle) {
#if (OCCA_OS & (OCCA_LINUX_OS | OCCA_OSX_OS))
      ::dlclose(dlHandle);
#else
      FreeLibrary((HMODULE) (dlHandle));
#endif
    }

    void runFunction(handleFunction_t f,
                     const int *occaKernelInfoArgs,
                     int occaInnerId0, int occaInnerId1, int occaInnerId2,
                     int argc, void **args) {

#include "operators/runFunctionFromArguments.cpp"
    }
  }

  mutex_t::mutex_t() {
#if (OCCA_OS & (OCCA_LINUX_OS | OCCA_OSX_OS))
    int error = pthread_mutex_init(&mutexHandle, NULL);

    OCCA_CHECK(error == 0,
               "Error initializing mutex");
#else
    mutexHandle = CreateMutex(NULL, FALSE, NULL);
#endif
  }

  void mutex_t::free() {
#if (OCCA_OS & (OCCA_LINUX_OS | OCCA_OSX_OS))
    int error = pthread_mutex_destroy(&mutexHandle);

    OCCA_CHECK(error == 0,
               "Error freeing mutex");
#else
    CloseHandle(mutexHandle);
#endif
  }

  void mutex_t::lock() {
#if (OCCA_OS & (OCCA_LINUX_OS | OCCA_OSX_OS))
    pthread_mutex_lock(&mutexHandle);
#else
    WaitForSingleObject(mutexHandle, INFINITE);
#endif
  }

  void mutex_t::unlock() {
#if (OCCA_OS & (OCCA_LINUX_OS | OCCA_OSX_OS))
    pthread_mutex_unlock(&mutexHandle);
#else
    ReleaseMutex(mutexHandle);
#endif
  }
}