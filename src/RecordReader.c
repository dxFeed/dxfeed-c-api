/*
* The contents of this file are subject to the Mozilla Public License Version
* 1.1 (the "License"); you may not use this file except in compliance with
* the License. You may obtain a copy of the License at
* http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
* for the specific language governing rights and limitations under the
* License.
*
* The Initial Developer of the Original Code is Devexperts LLC.
* Portions created by the Initial Developer are Copyright (C) 2010
* the Initial Developer. All Rights Reserved.
*
* Contributor(s):
*
*/

#include "RecordReader.h"

//dx_result_t readRecord(BufferedInput in, RecordBuffer buffer, int cipher, String symbol) {
//    long limit = buffer.getLimit();
//    incoming_record.readRecord(in, buffer, cipher, symbol);
//    RecordCursor read_cursor = buffer.cursorAt(limit);
//    RecordCursor write_cursor = buffer.add(resulting_record, cipher, symbol);
//    for (int i = 0; i < imap.length; i++) {
//        int j = imap[i];
//        if (j >= 0) {
//            int value = read_cursor.getInt(j);
//            if (iconv[i] != null)
//                write_cursor.setInt(i, iconv[i].convert(value));
//            else
//                write_cursor.setInt(i, value);
//        } else
//            write_cursor.setInt(i, 0);
//    }
//    for (int i = 0; i < omap.length; i++) {
//        int j = omap[i];
//        if (j >= 0) {
//            Object value = read_cursor.getObj(j);
//            if (oconv[i] != null)
//                write_cursor.setObj(i, oconv[i].convert(value));
//            else
//                write_cursor.setObj(i, value);
//        } else
//            write_cursor.setObj(i, null);
//    }
//    buffer.removeAt(limit);
//}
