/*
 * Copyright (C) 1996-2024 The Squid Software Foundation and contributors
 *
 * Squid software is distributed under GPLv2+ license and includes
 * contributions from numerous individuals and organizations.
 * Please see the COPYING and CONTRIBUTORS files for details.
 */

/* DEBUG: section 86    ESI processing */

#ifndef SQUID_SRC_ESI_EXPRESSION_H
#define SQUID_SRC_ESI_EXPRESSION_H

class ESIExpression
{

public:
    static int Evaluate (char const *);
};

#endif /* SQUID_SRC_ESI_EXPRESSION_H */

