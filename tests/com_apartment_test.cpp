#include "platform/windows/com_apartment.h"

#include <gtest/gtest.h>

#include <thread>
#include <type_traits>

namespace {

using audiohole::windows::ApartmentModel;
using audiohole::windows::ComApartment;

static_assert(!std::is_copy_constructible_v<ComApartment>);
static_assert(!std::is_copy_assignable_v<ComApartment>);
static_assert(!std::is_move_constructible_v<ComApartment>);
static_assert(!std::is_move_assignable_v<ComApartment>);

// Each test owns a fresh thread: runner/framework COM initialization must not
// influence the expected S_OK, S_FALSE, or RPC_E_CHANGED_MODE results.
TEST(ComApartment, InitializesMtaAndReleasesItOnScopeExit) {
    std::thread([] {
        {
            const ComApartment apartment;
            ASSERT_TRUE(apartment.initialized());
            EXPECT_EQ(apartment.result(), S_OK);
            APTTYPE type;
            APTTYPEQUALIFIER qualifier;
            ASSERT_EQ(CoGetApartmentType(&type, &qualifier), S_OK);
            EXPECT_EQ(type, APTTYPE_MTA);
        }

        // Switching to STA succeeds only if the MTA initialization was balanced.
        const HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        EXPECT_EQ(result, S_OK);
        if (SUCCEEDED(result)) {
            CoUninitialize();
        }
    }).join();
}

TEST(ComApartment, BalancesNestedSuccessfulInitialization) {
    std::thread([] {
        {
            const ComApartment outer;
            ASSERT_EQ(outer.result(), S_OK);
            {
                const ComApartment inner;
                EXPECT_EQ(inner.result(), S_FALSE);
                EXPECT_TRUE(inner.initialized());
            }
            // Destroying the inner scope must leave the outer MTA intact.
            const ComApartment incompatible(ApartmentModel::single_threaded);
            EXPECT_EQ(incompatible.result(), RPC_E_CHANGED_MODE);
        }
        // Fails if S_FALSE was mistakenly treated as not owning a COM count.
        const ComApartment sta(ApartmentModel::single_threaded);
        EXPECT_EQ(sta.result(), S_OK);
    }).join();
}

TEST(ComApartment, FailedInitializationDoesNotUninitializeExistingApartment) {
    std::thread([] {
        {
            const ComApartment sta(ApartmentModel::single_threaded);
            ASSERT_EQ(sta.result(), S_OK);
            {
                const ComApartment incompatible;
                EXPECT_EQ(incompatible.result(), RPC_E_CHANGED_MODE);
                EXPECT_FALSE(incompatible.initialized());
            }
            // A failed scope must not remove somebody else's initialization.
            const ComApartment still_incompatible;
            EXPECT_EQ(still_incompatible.result(), RPC_E_CHANGED_MODE);
        }
        const ComApartment mta;
        EXPECT_EQ(mta.result(), S_OK);
    }).join();
}

TEST(ComApartment, ReleasesInitializationDuringExceptionUnwinding) {
    std::thread([] {
        try {
            const ComApartment apartment;
            ASSERT_EQ(apartment.result(), S_OK);
            throw 42;
        } catch (int) {
            const ComApartment sta(ApartmentModel::single_threaded);
            EXPECT_EQ(sta.result(), S_OK);
        }
    }).join();
}

}  // namespace
