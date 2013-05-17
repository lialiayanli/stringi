/* This file is part of the 'stringi' library.
 * 
 * Copyright 2013 Marek Gagolewski, Bartek Tartanus, Marcin Bujarski
 * 
 * 'stringi' is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * 'stringi' is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with 'stringi'. If not, see <http://www.gnu.org/licenses/>.
 */
 
#include "stringi.h"




/** Duplicate given strings
 * 
 *
 *  @param s character vector
 *  @param c integer vector == times
 *  @return character vector;
 * 
 *  The function is vectorized over s and c
 *  if s is NA or c is NA the result will be NA
 *  if c<0 the result will be NA
 *  if c==0 the result will be an empty string
 *  if s or c is an empty vector then the result is an empty vector
 *  
 * @version 0.1 (Marek Gagolewski)
 * @version 0.2 (Marek Gagolewski) - use StriContainerUTF8's vectorization
*/
SEXP stri_dup(SEXP s, SEXP c)
{
   s = stri_prepare_arg_string(s); // prepare string argument
   c = stri_prepare_arg_integer(c); // prepare string argument
   R_len_t nc = LENGTH(c);
   R_len_t nmax = stri__recycling_rule(true, 2, LENGTH(s), nc);
   if (nmax <= 0) return allocVector(STRSXP, 0);
   
   int* cc = INTEGER(c);
   StriContainerUTF8* ss = new StriContainerUTF8(s, nmax);
   
   // STEP 1.
   // Calculate the required buffer length
   R_len_t bufsize = 0;
   for (R_len_t i=0; i<nmax; ++i) {
      if (ss->isNA(i) || cc[i % nc] == NA_INTEGER)
         continue;
         
      R_len_t cursize = cc[i % nc]*ss->get(i).size();
      if (cursize > bufsize)
         bufsize = cursize;
   }
   
   // STEP 2.
   // Alloc buffer & result vector
   char* buf = new char[bufsize+1];   
   SEXP ret;
   PROTECT(ret = allocVector(STRSXP, nmax));

   // STEP 3.
   // Duplicate
   const String8* last_string = NULL; // this will allow for reusing buffer...
   R_len_t last_index = 0;                // ...useful for stri_dup('a', 1:1000) or stri_dup('a', 1000:1)
   
   for (R_len_t i = ss->vectorize_init(); // this iterator allows for...
         i != ss->vectorize_end();        // ...smart buffer reusage
         i = ss->vectorize_next(i))
   {
      R_len_t cur_dups = cc[i % nc];
      if (ss->isNA(i) || cur_dups == NA_INTEGER || cur_dups < 0)
         SET_STRING_ELT(ret, i, NA_STRING);
      else {
         const String8* cur_string = &ss->get(i);
         R_len_t cur_length = cur_string->length();
         if (cur_dups <= 0 || cur_length <= 0) {
            SET_STRING_ELT(ret, i, mkCharLen("", 0));
            continue;
         }
         
         // all right, here the result will neither be NA nor an empty string
         
         if (cur_string != last_string) {
            // well, no reuse possible - resetting
            last_string = cur_string;
            last_index = 0;
         }
         
         // we paste only "additional" duplicates
         R_len_t max_index = cur_length*cur_dups;
         for (; last_index < max_index; last_index += cur_length) {
            memcpy(buf+last_index, cur_string->c_str(), cur_length);
         }
         
         // the result is always in UTF-8
         SET_STRING_ELT(ret, i, mkCharLenCE(buf, max_index, CE_UTF8));
      }
   }
   
   
   // STEP 4.
   // Clean up & finish
   
   delete buf;
   delete ss;
   UNPROTECT(1);
   return ret;
}




/** Join two character vectors, element by element
 * 
 * Vectorized over s1 and s2. Optimized for |s1| >= |s2|
 * @param s1 character vector
 * @param s2 character vector
 * @return character vector, res_i=s1_i + s2_i for |s1|==|s2|
 *  if s1 or s2 is NA then result is NA
 *  if s1 or s2 is empty, then the result is just s1 or s2
 *  
 *  TO DO: Encoding!!!
 * @version 0.1 (Marek Gagolewski)
*/
SEXP stri_join2(SEXP s1, SEXP s2)
{
   s1 = stri_prepare_arg_string(s1); // prepare string argument
   s2 = stri_prepare_arg_string(s2); // prepare string argument
   
   R_len_t ns1 = LENGTH(s1);
   R_len_t ns2 = LENGTH(s2);
   R_len_t nsm = stri__recycling_rule(true, 2, ns1, ns2);
   
   if (ns1 <= 0) return s2;
   if (ns2 <= 0) return s1;
   
   // find maximal length of the buffer needed
   R_len_t maxsize = 0;
   for (int i=0; i<ns1; ++i) {
      SEXP curs1 = STRING_ELT(s1, i);
      if (curs1 == NA_STRING) continue;
      R_len_t cnsl = LENGTH(curs1);
      
      for (int j=i; j<nsm; j+=ns1) {
         SEXP curs2 = STRING_ELT(s2, j%ns2);
         if (curs2 == NA_STRING) continue;
         R_len_t cns2 = LENGTH(curs2);
         
         if (cnsl+cns2 > maxsize) maxsize = cnsl+cns2;
      }
   }
   
   SEXP e;
   PROTECT(e = allocVector(STRSXP, nsm)); // output vector
      
   // it may happen that maxsize == 0!
   if (maxsize <= 0) {
      // => we'll have NA_character_ or "" in the output vector
      for (int i=0; i<nsm; ++i) {
         if (STRING_ELT(s1, i%ns1) == NA_STRING
          || STRING_ELT(s2, i%ns2) == NA_STRING)
            SET_STRING_ELT(e, i, NA_STRING);
         else
            SET_STRING_ELT(e, i, mkCharLen("", 0));
      }
   }
   else {
      char* buf = R_alloc(maxsize, sizeof(char)); // to be thread-safe
      char* buf2;
      
      // at least one string should be copied (for some i, j)
      for (int i=0; i<ns1; ++i) {
         SEXP ss1 = STRING_ELT(s1, i);
         if (ss1 == NA_STRING) {
            for (int j=i; j<nsm; j+=ns1)
               SET_STRING_ELT(e, j, NA_STRING); // NA %+% ??? == NA
         }
         else {
            buf2 = buf; // don't copy ss1 yet (copy only when needed)
         
            const char* si1 = CHAR(ss1); // this is not NA
            R_len_t sn1 = LENGTH(ss1);   // this may be equal to 0
               
            for (int j=i; j<nsm; j+=ns1) {  
               SEXP ss2 = STRING_ELT(s2, j%ns2);
               if (ss2 == NA_STRING)
                  SET_STRING_ELT(e, j, NA_STRING);
               else {
                  const char* si2 = CHAR(ss2); // this is not NA
                  R_len_t sn2 = LENGTH(ss2);   // this may be equal to 0
                  
                  if (sn1 != 0 && buf2 == buf) {
                     // now copy of ss1 is needed
                     memcpy(buf, si1, sn1);
                     buf2 = buf + sn1;
                  }
                  if (sn2 != 0)
                     memcpy(buf2, si2, sn2);
                  SET_STRING_ELT(e, j, mkCharLen(buf, sn1+sn2)); // sn1+sn2 may be equal to 0
               }
            }
         }
      }
   }
   
   UNPROTECT(1);
   return e;
}




/**
 * ....
 * @param s ...
 * @return ...
 */
SEXP stri_join(SEXP s)
{
   warning("stri_join is not finished!");
   // MG:
   // See Sec. 5.9.10 in http://cran.r-project.org/doc/manuals/R-exts.html
   // "Currently all arguments to a .Call call will have NAMED set to 2, 
   // and so users must assume that they need to be duplicated before alteration"
   // NAMED set to 2 -> The object has potentially been bound to two or 
   // more symbols, and one should act as if another variable is currently bound to this value. 
   SEXP str = duplicate(s);
   int slen = LENGTH(str);
   int max = 0;
   int* elementslen = (int*)R_alloc(slen, sizeof(int)); 
   for(int i=0;i<slen;++i){
      //prepare each element of the list
      SET_VECTOR_ELT(str,i,stri_prepare_arg_string(VECTOR_ELT(str,i)));
      //save length of each element for further operations
      elementslen[i] = LENGTH(VECTOR_ELT(str,i));
      //check maximum size
      if(max < elementslen[i]) max = elementslen[i];
   }
   for(int i=0;i<slen;++i){
      if(max % elementslen[i]!=0){
         warning(MSG__WARN_RECYCLING_RULE);
         break;
      }
   }
   SEXP e;
   PROTECT(e = allocVector(STRSXP,max));
   const char* buf = R_alloc(5, sizeof(char));
   char* temp = R_alloc(5, sizeof(char));
   char* ret = R_alloc(10, sizeof(char));
   for(int i=0;i<max;++i){
      memcpy(ret, CHAR(STRING_ELT(VECTOR_ELT(str,0),i%elementslen[0])),5);
      for(int j=1;j<slen;++j){
         buf = CHAR(STRING_ELT(VECTOR_ELT(str,j),i%elementslen[j]));
         memcpy(temp,buf,5);
         strcat(ret, temp);
         //join strings from each list element
      }
      SET_STRING_ELT(e,i,mkCharLen(ret,10));
   }
   UNPROTECT(1);
   return e;
}




/** String vector flatten, with no separator
 *
 *  if any of s is NA, the result will be NA_character_
 * 
 *  @param s character vector
 *  @return if s is not empty, then a character vector of length 1 
 * 
 * @version 0.1 (Marek Gagolewski)
 * @version 0.2 (Marek Gagolewski) - StriContainerUTF8 - any R Encoding
 */
SEXP stri_flatten_nosep(SEXP s)
{
   s = stri_prepare_arg_string(s);
   
   R_len_t ns = LENGTH(s);
   if (ns <= 0) return s;
   
   StriContainerUTF8* ss = new StriContainerUTF8(s, ns);
   
   // 1. Get required buffer size
   R_len_t nchar = 0;
   for (int i=0; i<ns; ++i) {
      if (ss->isNA(i)) {
         delete ss;
         return stri__vector_NA_strings(1); // at least 1 NA => return NA
      }
      nchar += ss->get(i).length();
   }
   
   // 2. Fill the buf!
   char* buf = new char[nchar]; // NULL not needed
   R_len_t cur = 0;
   for (int i=0; i<ns; ++i) {
      R_len_t ncur = ss->get(i).length();
      memcpy(buf+cur, ss->get(i).c_str(), ncur);
      cur += ncur;
   }
   
   
   // 3. Get ret val & solongfarewellaufwiedersehenadieu
   SEXP ret; 
   PROTECT(ret = allocVector(STRSXP, 1));
   SET_STRING_ELT(ret, 0, mkCharLenCE(buf, nchar, CE_UTF8));
   delete buf;
   delete ss;
   UNPROTECT(1);
   return ret;
}


/** String vector flatten, with separator possible between each string
 *
 *  if any of s is NA, the result will be NA_character_
 * 
 *  @param s character vector
 *  @param sep a single string [R name: collapse]
 *  @return if s is not empty, then a character vector of length 1 
 * 
 * @version 0.1 (Marek Gagolewski)
 * @version 0.2 (Bartek Tartanus) - sep arg added (1 sep supported)
 * @version 0.3 (Marek Gagolewski) - StriContainerUTF8 - any R Encoding
 */
SEXP stri_flatten(SEXP s, SEXP sep)
{
   // Check if sep is given?
   sep = stri_prepare_arg_string(sep);
//   if (LENGTH(sep) == 1 && LENGTH(STRING_ELT(sep, 0)) == 0)
//      return stri_flatten_nosep(s); // specialized -> slightly (not too much) faster
      
   s = stri_prepare_arg_string(s); // prepare string argument   
   R_len_t ns = LENGTH(s);
   R_len_t nsep = LENGTH(sep);
   if (ns <= 0 || nsep <= 0) return stri__vector_empty_strings(0);
   
   if (nsep > 1) {
      warning(MSG__COLLAPSE_EXPECTED1);
      SEXP sep_old = sep;
      PROTECT(sep = allocVector(STRSXP, 1));
      SET_STRING_ELT(sep, 0, STRING_ELT(sep_old, 0));
      UNPROTECT(1);
   }
   
   StriContainerUTF8* ssep = new StriContainerUTF8(sep, 1);
   if (ssep->isNA(0)) {
      delete ssep;
      return stri__vector_NA_strings(1);
   }
   
   R_len_t ncharsep = ssep->get(0).length();
   
   StriContainerUTF8* ss = new StriContainerUTF8(s, ns);
   
   // 1. Get required buffer size
   R_len_t nchar = 0;
   for (int i=0; i<ns; ++i) {
      if (ss->isNA(i)) {
         delete ssep;
         delete ss;
         return stri__vector_NA_strings(1); // at least 1 NA => return NA
      }
      nchar += ss->get(i).length() + ((i<ns-1)?ncharsep:0);
   }

   
   // 2. Fill the buf!
   char* buf = new char[nchar]; // NULL not needed
   R_len_t cur = 0;
   for (int i=0; i<ns; ++i) {
      R_len_t ncur = ss->get(i).length();
      memcpy(buf+cur, ss->get(i).c_str(), ncur);
      cur += ncur;
      if (i < ns-1 && ncharsep > 0) {
         memcpy(buf+cur, ssep->get(0).c_str(), ncharsep);
         cur += ncharsep;  
      }
   }
   
   
   // 3. Get ret val & solongfarewellaufwiedersehenadieu
   SEXP ret; 
   PROTECT(ret = allocVector(STRSXP, 1));
   SET_STRING_ELT(ret, 0, mkCharLenCE(buf, nchar, CE_UTF8));
   delete buf;
   delete ssep;
   delete ss;
   UNPROTECT(1);
   return ret;
}

