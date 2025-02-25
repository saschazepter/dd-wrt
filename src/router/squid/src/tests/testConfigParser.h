/*
 * Copyright (C) 1996-2024 The Squid Software Foundation and contributors
 *
 * Squid software is distributed under GPLv2+ license and includes
 * contributions from numerous individuals and organizations.
 * Please see the COPYING and CONTRIBUTORS files for details.
 */

#ifndef SQUID_SRC_TESTS_TESTCONFIGPARSER_H
#define SQUID_SRC_TESTS_TESTCONFIGPARSER_H

#include "compat/cppunit.h"

/*
 * test the ConfigParser framework
 */

class testConfigParser : public CPPUNIT_NS::TestFixture
{
    CPPUNIT_TEST_SUITE( testConfigParser );
    CPPUNIT_TEST( testParseQuoted );
    CPPUNIT_TEST_SUITE_END();

public:
    void setUp() override;

protected:
    bool doParseQuotedTest(const char *, const char *);
    void testParseQuoted();
};

#endif /* SQUID_SRC_TESTS_TESTCONFIGPARSER_H */

