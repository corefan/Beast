//
// Copyright (c) 2013-2016 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Test that header file is self-contained.
#include <beast/core/handler_ptr.hpp>

#include <beast/unit_test/suite.hpp>

namespace beast {

struct handler_ptr_test : beast::unit_test::suite
{
    struct H
    {
        void
        operator()()
        {
        }
    };

    struct D
    {
        int i;

        D(int i_)
            : i(i_)
        {
        }
    };

    void
    test()
    {
        handler_ptr<D, H> p1;
        BEAST_EXPECT(! p1);
        handler_ptr<D, H> p2(H(), 1);
        BEAST_EXPECT(p2);
    }

    void
    run() override
    {
        test();
    }
};

BEAST_DEFINE_TESTSUITE(handler_ptr,core,beast);

} // beast