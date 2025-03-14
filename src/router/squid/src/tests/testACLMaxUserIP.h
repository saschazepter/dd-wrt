/*
 * Copyright (C) 1996-2024 The Squid Software Foundation and contributors
 *
 * Squid software is distributed under GPLv2+ license and includes
 * contributions from numerous individuals and organizations.
 * Please see the COPYING and CONTRIBUTORS files for details.
 */

#ifndef SQUID_SRC_TESTS_TESTACLMAXUSERIP_H
#define SQUID_SRC_TESTS_TESTACLMAXUSERIP_H

#if USE_AUTH

#include "compat/cppunit.h"

/*
 * demonstration test file, as new idioms are made they will
 * be shown in the testBoilerplate source.
 */

class testACLMaxUserIP : public CPPUNIT_NS::TestFixture
{
    CPPUNIT_TEST_SUITE( testACLMaxUserIP );
    /* note the statement here and then the actual prototype below */
    CPPUNIT_TEST( testDefaults );
    CPPUNIT_TEST( testParseLine );
    CPPUNIT_TEST_SUITE_END();

public:
    void setUp() override;

protected:
    void testDefaults();
    void testParseLine();
};

#endif /* USE_AUTH */
#endif /* SQUID_SRC_TESTS_TESTACLMAXUSERIP_H */

