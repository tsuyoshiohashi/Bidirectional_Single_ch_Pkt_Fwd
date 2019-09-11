/*
* jsonjob.c
* Copyright (c) 2019 Tsuyoshi Ohashi
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
*/

#include <stdio.h>
#include <string.h> // memset

#include "jsonjob.h"
#include "parson.h"
#include "base64.h"

rf_txpkt_s txpkt;

// find rf tx data and set in txpkt
void cut_rftxdata(uint8_t* buf_down){
    
    /* JSON parsing variables */
    JSON_Value *root_val = NULL;
    JSON_Object *txpk_obj = NULL;
    JSON_Value *val = NULL; /* needed to detect the absence of some fields */
    const char *str; /* pointer to sub-strings in the JSON data */
    int i; /* loop variables */

    /* initialize TX struct and try to parse JSON */
    memset(&txpkt, 0, sizeof txpkt);
    root_val = json_parse_string_with_comments((const char *)(buf_down + 4)); /* JSON offset */
    if (root_val == NULL) {
        printf("WARNING: [down] invalid JSON, TX aborted\n");
    }

    /* look for JSON sub-object 'txpk' */
    txpk_obj = json_object_get_object(json_value_get_object(root_val), "txpk");
    if (txpk_obj == NULL) {
        printf("WARNING: [down] no \"txpk\" object in JSON, TX aborted\n");
            json_value_free(root_val);
    }

    /* Parse payload length (mandatory) */
    val = json_object_get_value(txpk_obj,"size");
    if (val == NULL) {
        printf("WARNING: [down] no mandatory \"txpk.size\" object in JSON, TX aborted\n");
        json_value_free(root_val);
    }
    txpkt.size = (uint16_t)json_value_get_number(val);

    /* Parse payload data (mandatory) */
    str = json_object_get_string(txpk_obj, "data");
    if (str == NULL) {
        printf("WARNING: [down] no mandatory \"txpk.data\" object in JSON, TX aborted\n");
        json_value_free(root_val);
    }
    //printf("txpk:%s \n", str);

    i = b64_to_bin(str, strlen(str), txpkt.payload, sizeof txpkt.payload);
    if (i != txpkt.size) {
        printf("WARNING: [down] mismatch between .size and .data size once converter to binary\n");
    }

    /* free the JSON parse tree from memory */
    json_value_free(root_val);
}
// end of jsonjob.c