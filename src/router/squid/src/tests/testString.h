/*
 * Copyright (C) 1996-2024 The Squid Software Foundation and contributors
 *
 * Squid software is distributed under GPLv2+ license and includes
 * contributions from numerous individuals and organizations.
 * Please see the COPYING and CONTRIBUTORS files for details.
 */

#ifndef SQUID_SRC_TESTS_TESTSTRING_H
#define SQUID_SRC_TESTS_TESTSTRING_H

#include "compat/cppunit.h"

/*
 * test the store framework
 */

class testString : public CPPUNIT_NS::TestFixture
{
    CPPUNIT_TEST_SUITE( testString );
    CPPUNIT_TEST( testCmpDefault );
    CPPUNIT_TEST( testCmpEmptyString );
    CPPUNIT_TEST( testCmpNotEmptyDefault );
    CPPUNIT_TEST( testSubstr );

    CPPUNIT_TEST_SUITE_END();

public:
    void setUp() override;

protected:
    void testCmpDefault();
    void testCmpEmptyString();
    void testCmpNotEmptyDefault();
    void testSubstr();
};

#endif /* SQUID_SRC_TESTS_TESTSTRING_H */

