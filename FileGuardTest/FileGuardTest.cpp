#include <windows.h>

#include <iostream>

#pragma warning(push)
#pragma warning(disable: 4423)
#include "CLI11/CLI11.hpp"
#pragma warning(pop)

#define FGT_MAJOR_VERSION ((USHORT)0)
#define FGT_MINOR_VERSION ((USHORT)1)
#define FGT_PATCH_VERSION ((USHORT)0)
#define FGT_BUILD_VERSION ((USHORT)0)

namespace fileguard {

    class Test {
    public:
        static std::unique_ptr<Test> New(int argc, wchar_t** argv) noexcept {
            std::unique_ptr<Test> test(new Test(argc, argv));
            return std::move(test);
        }

        ~Test() = default;

        HRESULT Parse() {
            return E_NOTIMPL;
        }

    private:
        int argc_;
        wchar_t** argv_;

        Test(int argc, wchar_t** argv) {
            argc_ = argc;
            argv_ = argv;
        }
    };
}

int wmain(int argc, wchar_t* argv[]) {
    return S_OK;
}