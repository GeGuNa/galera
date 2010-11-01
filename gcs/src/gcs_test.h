/*
 * Copyright (C) 2008 Codership Oy <info@codership.com>
 *
 * $Id$
 */

#ifndef _gcs_test_h_
#define _gcs_test_h_

// some data to test bugger packets
static
char gcs_test_data[] = 
"001 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"002 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"003 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"004 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"005 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"006 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"007 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"008 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"009 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"010 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"011 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"012 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"013 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"014 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"015 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"016 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"017 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"018 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"019 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"020 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"021 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"022 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"023 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"024 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"025 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"026 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"027 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"028 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"029 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"030 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"031 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"032 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"033 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"034 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"035 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"036 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"037 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"038 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"039 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"040 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"041 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"042 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"043 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"044 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"045 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"046 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"047 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"048 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"049 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"050 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"051 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"052 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"053 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"054 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"055 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"056 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"057 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"058 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"059 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"060 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"061 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"062 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"063 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"064 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"065 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"066 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"067 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"068 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"069 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"070 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"071 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"072 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"073 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"074 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"075 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"076 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"077 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"078 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"079 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"080 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"081 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"082 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"083 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"084 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"085 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"086 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"087 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"088 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"089 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"090 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"091 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"092 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"093 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"094 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"095 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"096 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"097 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"098 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"099 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"100 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
;
#endif