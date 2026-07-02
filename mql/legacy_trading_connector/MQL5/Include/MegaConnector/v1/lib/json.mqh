// $Id: json.mqh 4 2015-06-24 13:11:09Z ydrol $
#ifndef YDROL_JSON_MQH
#define YDROL_JSON_MQH

// (C)2014 Andrew Lord forex@NICKNAME@lordy.org.uk
// Parse a JSON String - Adapted for mql4++ from my gawk implementation
// ( https://code.google.com/p/oversight/source/browse/trunk/bin/catalog/json.awk )

/*
   TODO the constants true|false|null could be represented as fixed objects.
      To do this the deleting of _hash and _array must skip these objects.

   TODO test null

   TODO Parse Unicode Escape
*/

//#define TOLERATE_TAB_CR_NL_IN_STRINGS

/*
   See json_demo for examples.

 This requires the hash.mqh ( http://codebase.mql4.com/9238 , http://lordy.co.nf/hash )



 */

#include "hash.mqh"

/// Different types of JSON Values
enum ENUM_JSON_TYPE { JSON_NULL, JSON_OBJECT , JSON_ARRAY, JSON_NUMBER, JSON_STRING , JSON_BOOL };

class JSONString ;

///
/// Generic class for all JSON types (Number, String, Bool, Array, Object )
/// It is a subclass of HashValue so it can be stored in an JSONObject hash
///
class JSONValue : public HashValue {
    private:
    ENUM_JSON_TYPE _type;
    int _pos, _line, _column;


    public:
        JSONValue(int pos=0, int line=0, int column=0) : _pos(pos), _line(line), _column(column) { }
        ~JSONValue() {}

        ENUM_JSON_TYPE getType() { return _type; }
        void setType(ENUM_JSON_TYPE t) { _type = t; }

        /// True if JSONValue is a instance of JSONString
        bool isString() { return _type == JSON_STRING; }

        /// True if JSONValue is a instance of JSONNull 
        bool isNull() { return _type == JSON_NULL; }

        /// True if JSONValue is a instance of JSONObject
        bool isObject() { return _type == JSON_OBJECT; }

        /// True if JSONValue is a instance of JSONArray
        bool isArray() { return _type == JSON_ARRAY; }

        /// True if JSONValue is a instance of JSONNumber
        bool isNumber() { return _type == JSON_NUMBER; }
        bool isInt() { return _type == JSON_NUMBER && ((JSONNumber*)GetPointer(this)).isInt(); }
        bool isIntType() { return _type == JSON_NUMBER && ((JSONNumber*)GetPointer(this)).isIntType(); }
        bool isLong() { return _type == JSON_NUMBER && ((JSONNumber*)GetPointer(this)).isLong(); }
        bool isLongType() { return _type == JSON_NUMBER && ((JSONNumber*)GetPointer(this)).isLongType(); }
        bool isDouble() { return _type == JSON_NUMBER && ((JSONNumber*)GetPointer(this)).isDouble(); }

        /// True if JSONValue is a instance of JSONBool
        bool isBool() { return _type == JSON_BOOL; }

        // Override in child classes
        virtual string toString() {
            return "";
        }

        // Some convenience getters to cast to the subtype. - this is bad OO design!

        /// If this JSONValue is an instance of JSONString return the string (or cast will fail)
        string getString() { return ((JSONString *)GetPointer(this)).getString(); }

        /// If this JSONValue is an instance of JSONNumber return the double (or cast will fail)
        double getDouble() { return ((JSONNumber *)GetPointer(this)).getDouble(); }

        /// If this JSONValue is an instance of JSONNumber return the long (or cast will fail)
        long getLong() { return ((JSONNumber *)GetPointer(this)).getLong(); }

        /// If this JSONValue is an instance of JSONNumber return the int (or cast will fail)
        int getInt() { return ((JSONNumber *)GetPointer(this)).getInt(); }

        /// If this JSONValue is an instance of JSONBool return the bool (or cast will fail)
        bool getBool() { return ((JSONBool *)GetPointer(this)).getBool(); }


        /// Get the string value of the JSONValue, without Program termination
        /// @param val : String object from which value will be extracted.
        /// @param out : The string than was extracted.
        /// @return true if OK else false
        static bool getString(JSONValue *val,string &out)
        {
            if (val != NULL && val.isString()) {
                out = val.getString();
                return true;
            }
            return false;
        }
        /// Get the bool value of the JSONValue, without Program termination
        /// @param val : String object from which value will be extracted.
        /// @param out : The bool than was extracted.
        /// @return true if OK else false
        static bool getBool(JSONValue *val,bool &out)
        {
            if (val != NULL && val.isBool()) {
                out = val.getBool();
                return true;
            }
            return false;
        }
        /// Get the double value of the JSONValue, without Program termination
        /// @param val : String object from which value will be extracted.
        /// @param out : The double than was extracted.
        /// @return true if OK else false
        static bool getDouble(JSONValue *val,double &out)
        {
            if (val != NULL && val.isDouble()) {
                out = val.getDouble();
                return true;
            }
            return false;
        }
        /// Get the long value of the JSONValue, without Program termination
        /// @param val : String object from which value will be extracted.
        /// @param out : The long than was extracted.
        /// @return true if OK else false
        static bool getLong(JSONValue *val,long &out)
        {
            if (val != NULL && val.isLong()) {
                out = val.getLong();
                return true;
            }
            return false;
        }
        static bool getLongType(JSONValue *val,long &out)
        {
            if (val != NULL && val.isLongType()) {
                out = val.getLong();
                return true;
            }
            return false;
        }
        /// Get the int value of the JSONValue, without Program termination
        /// @param val : String object from which value will be extracted.
        /// @param out : The int than was extracted.
        /// @return true if OK else false
        static bool getInt(JSONValue *val,int &out)
        {
            if (val != NULL && val.isInt()) {
                out = val.getInt();
                return true;
            }
            return false;
        }
        static bool getIntType(JSONValue *val,int &out)
        {
            if (val != NULL && val.isIntType()) {
                out = val.getInt();
                return true;
            }
            return false;
        }
        
        static bool isString(JSONValue *val)
        {
            return val != NULL && val.isString();
        }

        static bool isBool(JSONValue *val)
        {
            return val != NULL && val.isBool();
        }

        static bool isNumber(JSONValue *val)
        {
            return val != NULL && val.isNumber();
        }

        static bool isDouble(JSONValue *val)
        {
            return val != NULL && val.isNumber() && ((JSONNumber*)val).isDouble();
        }

        static bool isLong(JSONValue *val)
        {
            return val != NULL && val.isNumber() && ((JSONNumber*)val).isLong();
        }

        static bool isLongType(JSONValue *val)
        {
            return val != NULL && val.isNumber() && ((JSONNumber*)val).isLongType();
        }

        static bool isInt(JSONValue *val)
        {
            return val != NULL && val.isNumber() && ((JSONNumber*)val).isInt();
        }

        static bool isIntType(JSONValue *val)
        {
            return val != NULL && val.isNumber() && ((JSONNumber*)val).isIntType();
        }

        static bool isArray(JSONValue *val)
        {
            return val != NULL && val.isArray();
        }

        static bool isObject(JSONValue *val)
        {
            return val != NULL && val.isObject();
        }

        static bool isNull(JSONValue *val)
        {
            return val != NULL && val.isNull();
        }

        int get_char_position()
        {
            return _pos;
        }
        int get_line()
        {
            return _line;
        }
        int get_column()
        {
            return _column;
        }
};

// -----------------------------------------

/// Class to represent a JSON String 
class JSONString : public JSONValue {
    private:
        string _string;
    public:
        JSONString(string s, int pos=0, int line=0, int column=0) : JSONValue(pos, line, column)
        {
            setString(s);
            setType(JSON_STRING);
        }
        JSONString(int pos=0, int line=0, int column=0) : JSONValue(pos, line, column)
        {
            setType(JSON_STRING);
        }
        string getString() { return _string; }
        void setString(string v) { _string = v; }
        string toString()
        {
            ushort uc;
            int slen=StringLen(_string), clen=0, i;
            for(i=0; i<slen; i++)
            {
               uc = StringGetCharacter(_string, i);
               if(uc==0x22 /* " */ || uc==0x5C /* \ */)
               {
                   clen += 2;   // \" or \\
               }
               else if(uc>=0x20)
               {
                   clen++;
               }
               else if(uc>=0x08 && uc<=0x0D && uc!=0x0B)
               {
                   /* backspace       U+0008 */
                   /* tab             U+0009 */
                   /* line feed       U+000A */
                   /* form feed       U+000C */
                   /* carriage return U+000D */
                   clen += 2;
               }
               else
               {
                   clen += 6;   // Unicode-Escape in the Basic Multilingual Plane (U+0000 bis U+FFFF), e.G. \u005C
               }
            }
            if(clen==slen)
            {
               return "\"" + _string + "\"";
            }
            
            string str;
            StringInit(str, clen);
            clen=0;
            for(i=0; i<slen; i++)
            {
               uc = StringGetCharacter(_string, i);
               if(uc==0x22 /* " */ || uc==0x5C /* \ */)
               {
                  StringSetCharacter(str, clen++, 0x5C /* \ */);
                  StringSetCharacter(str, clen++, uc);
               }
               else if(uc>=0x20)
               {
                  StringSetCharacter(str, clen++, uc);
               }
               else if(uc>=0x08 && uc<=0x0D && uc!=0x0B)
               {
                  StringSetCharacter(str, clen++, 0x5C /* \ */);
                  if(0x08==uc)
                  {  /* backspace       U+0008 */
                     StringSetCharacter(str, clen++, 'b');
                  }
                  else if(0x09==uc)
                  {  /* tab             U+0009 */
                     StringSetCharacter(str, clen++, 't');
                  }
                  else if(0x0A==uc)
                  {  /* line feed       U+000A */
                     StringSetCharacter(str, clen++, 'n');
                  }
                  else if(0x0C==uc)
                  {  /* form feed       U+000C */
                     StringSetCharacter(str, clen++, 'f');
                  }
                  else if(0x0D==uc)
                  {  /* carriage return U+000D */
                     StringSetCharacter(str, clen++, 'r');
                  }
               }
               else
               {
                  StringSetCharacter(str, clen++, 0x5C /* \ */);
                  StringSetCharacter(str, clen++, 0x75 /* u */);
                  for(int j=0; j<4; j++)
                  {
                     ushort hex = (uc & 0xF000) >> 12;
                     if(hex < 10)
                     {
                        StringSetCharacter(str, clen++, (ushort)((ushort)'0' + hex));
                     }
                     else
                     {
                        StringSetCharacter(str, clen++, (ushort)(((ushort)'A' - 10) + hex));
                     }
                     uc = (uc & 0x0FFF) << 4;
                  }
               }
            }
            
            return "\"" + str +"\"";
        }
};


// -----------------------------------------

/// Class to represent a JSON Bool 
class JSONBool : public JSONValue {
    private:
        bool _bool;
    public:
        JSONBool(bool b, int pos=0, int line=0, int column=0) : JSONValue(pos, line, column)
        {
            setBool(b);
            setType(JSON_BOOL);
        }
        JSONBool(int pos=0, int line=0, int column=0) : JSONValue(pos, line, column)
        {
            setType(JSON_BOOL);
        }
        bool getBool() { return _bool; }
        void setBool(bool v) { _bool = v; }
        string toString() { return (string)_bool; }
};

// -----------------------------------------

/// A JSON number may be internall replresented as either an MQL4 double or a long depending on how it was parsed. 
/// If one type is set the other is zeroed.
class JSONNumber : public JSONValue {
    private:
        long _long;
        double _dbl;
        bool type_is_double;
    public:
        JSONNumber(long l, int pos=0, int line=0, int column=0) : JSONValue(pos, line, column)
        {
            _long = l;
            _dbl = 0;
            type_is_double = false;
            setType(JSON_NUMBER);
        }
        JSONNumber(double d, int pos=0, int line=0, int column=0) : JSONValue(pos, line, column)
        {
            _long = 0;
            _dbl = d;
            type_is_double = true;
            setType(JSON_NUMBER);
        }
        /// Get the long value, (cast) from internal double if necessary.
        long getLong() {
            if(type_is_double) {
                return (long)_dbl;
            } else {
                return _long;
            }
        }
        /// Get the int value, (cast) from internal value.
        int getInt() {
            if (type_is_double) {
                return (int)_dbl;
            } else {
                return (int)_long;
            }
        }
        /// Get the double value, (cast) from internal long if necessary.
        double getDouble() 
        {
            if (!type_is_double) {
                return (double)_long;
            } else {
                return _dbl;
            }
        }
        bool isIntType() {  // Is int type (e.g. parsed from 5 but not from 5.0) ?
            return false==type_is_double && _long==((long)((int)_long));
        }
        bool isInt() {  // Is int type or convertible lossless to int ?
            return isIntType() || (true==type_is_double && _dbl==((double)((int)_dbl)));
        }
        bool isLongType() {  // Is long type (e.g. parsed from 5 but not from 5.0) ?
            return false==type_is_double;
        }
        bool isLong() {  // Is long type or convertible lossless to long ?
            return false==type_is_double || (true==type_is_double && _dbl==((double)((long)_dbl)));
        }
        bool isDouble() {  // Is double type or convertible lossless to double ?
            // Returns true in most cases, but for large long numbers a conversion to double may not be lossless
            // since the number of mantissa bits of a double is less than 64, but a long type has 64 bits.
            return type_is_double || _long==((long)((double)_long));
        }
        bool isDoubleType() {  // Is double type (e.g. parsed from 5.0 but not from 5) ?
            return type_is_double;
        }
        string toString() {
            if (type_is_double)
            {
                string str = (string)_dbl;
                if(StringFind(str, ".")>=0 || StringFind(str, "E")>=0 || StringFind(str, "e")>=0)
                {
                    if(StringLen(str)>=2 && (StringGetCharacter(str, 0)=='+' || StringGetCharacter(str, 0)=='-') && StringGetCharacter(str, 1)=='.')
                    {
                       str = StringSubstr(str, 0, 1) + "0" + StringSubstr(str, 1);  // JSON does not allow the notation e.g. .5 instead of 0.5 .
                    }
                    else if(StringLen(str)>=1 && StringGetCharacter(str, 0)=='.')
                    {
                       str = "0" + str;  // JSON does not allow the notation e.g. .5 instead of 0.5 .
                    }
                    return str;
                }
                else
                {
                    return str + ".0";
                }
            }
            else
            {
                return (string)_long;
            }
        }
};
// -----------------------------------------


/// This class should not be necessary, but null is genrally infrequent so
/// I havent bothered to code it away yet.
class JSONNull : public JSONValue {
    public:
    JSONNull(int pos=0, int line=0, int column=0) : JSONValue(pos, line, column)
    {
        setType(JSON_NULL);
    }
    ~JSONNull() {}
    string toString() 
    {
        return "null";
    }
};

//forward declaration
class JSONArray ;

/// This represents a JSONObject which is represented internally as a Hash
class JSONObject : public JSONValue {
private:
    Hash *_hash;

public:
        
        JSONObject(int pos=0, int line=0, int column=0) : JSONValue(pos, line, column)
        {
            setType(JSON_OBJECT);
            _hash = NULL;
        }
        
        ~JSONObject()
        {
            if (_hash != NULL) delete _hash;
        }
        
        /// Lookup key and get associated string value - halt program if wrong type(cast error) or doesnt exist(null pointer)
        string getString(string key) 
        {
            return getValue(key).getString();
        }
        /// Lookup key and get associated bool value - halt program if wrong type(cast error) or doesnt exist(null pointer)
        bool getBool(string key) 
        {
            return getValue(key).getBool();
        }
        /// Lookup key and get associated double value - halt program if wrong type(cast error) or doesnt exist(null pointer)
        double getDouble(string key) 
        {
            return getValue(key).getDouble();
        }
        /// Lookup key and get associated long value - halt program if wrong type(cast error) or doesnt exist(null pointer)
        long getLong(string key) 
        {
            return getValue(key).getLong();
        }
        /// Lookup key and get associated int value - halt program if wrong type(cast error) or doesnt exist(null pointer)
        int getInt(string key) 
        {
            return getValue(key).getInt();
        }

        bool isString(string key)
        {
            return JSONValue::isString(getValue(key));
        }
        
        bool isNumber(string key)
        {
            return JSONValue::isNumber(getValue(key));
        }
        
        bool isDouble(string key)
        {
            return JSONValue::isDouble(getValue(key));
        }
        bool isInt(string key)
        {
            return JSONValue::isInt(getValue(key));
        }
        
        bool isIntType(string key)
        {
            return JSONValue::isIntType(getValue(key));
        }
        bool isLong(string key)
        {
            return JSONValue::isLong(getValue(key));
        }
        bool isLongType(string key)
        {
            return JSONValue::isLongType(getValue(key));
        }
        
        bool isBool(string key)
        {
            return JSONValue::isBool(getValue(key));
        }
        
        bool isArray(string key)
        {
            return JSONValue::isArray(getValue(key));
        }
        
        bool isObject(string key)
        {
            return JSONValue::isObject(getValue(key));
        }
        
        bool isNull(string key)
        {
            return JSONValue::isNull(getValue(key));
        }

        /// Lookup key and get associated string value, return false if failure.
        bool getString(string key,string &out)
        {
            return JSONValue::getString(getValue(key),out);
        }
        /// Lookup key and get associated bool value, return false if failure.
        bool getBool(string key,bool &out)
        {
            return JSONValue::getBool(getValue(key),out);
        }
        /// Lookup key and get associated double value, return false if failure.
        bool getDouble(string key,double &out)
        {
            return JSONValue::getDouble(getValue(key),out);
        }
        /// Lookup key and get associated long value, return false if failure.
        bool getLong(string key,long &out)
        {
            return JSONValue::getLong(getValue(key),out);
        }
        bool getLongType(string key,long &out)
        {
            return JSONValue::getLongType(getValue(key),out);
        }
        /// Lookup key and get associated int value, return false if failure.
        bool getInt(string key,int &out)
        {
            return JSONValue::getInt(getValue(key),out);
        }
        bool getIntType(string key,int &out)
        {
            return JSONValue::getIntType(getValue(key),out);
        }

        /// Lookup key and get associated array, NULL if not present. Cast failure if not an Array.
        JSONArray *getArray(string key) 
        {
            return getValue(key);
        }
        /// Lookup key and get associated Object, NULL if not present. Cast failure if not an Object.
        JSONObject *getObject(string key) 
        {
            return getValue(key);
        }
        /// Lookup key and get associated value - best for data whose structure might change as any type can safely be returned.
        JSONValue *getValue(string key) 
        {
            if (_hash == NULL) {
                return NULL;
            }
            return (JSONValue*)_hash.hGet(key);
        }

        /// Store the value against the specified key string - Used by the parser.
        void put(string key,JSONValue *v)
        {
            if (_hash == NULL) _hash = new Hash();
            _hash.hPut(key,v);
        }
        string toString() {
           string s = "{";
           if (_hash != NULL) {
               HashLoop *l;
               int n=0;
               
               for(l = new HashLoop(_hash) ; l.isValid() ; l.next() ) {
                   JSONValue *v = (JSONValue *)(l.val());
                   s = s + (++n==1?"":",") + "\"" + l.key() + "\" : " + v.toString();
               }
               delete l;
           }
           s = s + "}";
           return s; 
        }

        /// Return the internal Hash - Used by JSONIterator
        Hash *getHash() {
            return _hash;
        }
};

/// This is a JSONArray which is represented internally as a MQL4 dynamic array of JSONValue * 
class JSONArray : public JSONValue {
    private:
        int _size;
        JSONValue *_array[];
    public:
        JSONArray(int pos=0, int line=0, int column=0) : JSONValue(pos, line, column)
        {
            setType(JSON_ARRAY);
            _size = 0;
        }
        ~JSONArray() {
            // clean up array
            for(int i = ArrayRange(_array,0)-1 ; i >= 0 ; i-- ) {
                if (CheckPointer(_array[i]) == POINTER_DYNAMIC ) delete _array[i];
            }
        }
        // Getters for Objects (key lookup ) --------------------------------------
        
        /// Lookup string value by array index - halt program if wrong type(cast error) or doesnt exist(null pointer)
        string getString(int index) 
        {
            return getValue(index).getString();
        }
        /// Lookup bool value by array index - halt program if wrong type(cast error) or doesnt exist(null pointer)
        bool getBool(int index) 
        {
            return getValue(index).getBool();
        }
        /// Lookup double value by array index - halt program if wrong type(cast error) or doesnt exist(null pointer)
        double getDouble(int index) 
        {
            return getValue(index).getDouble();
        }
        /// Lookup long value by array index - halt program if wrong type(cast error) or doesnt exist(null pointer)
        long getLong(int index) 
        {
            return getValue(index).getLong();
        }
        /// Lookup int value by array index - halt program if wrong type(cast error) or doesnt exist(null pointer)
        int getInt(int index) 
        {
            return getValue(index).getInt();
        }
        
        bool isString(int index)
        {
            return JSONValue::isString(getValue(index));
        }
        bool isNumber(int index)
        {
            return JSONValue::isNumber(getValue(index));
        }
        bool isDouble(int index)
        {
            return JSONValue::isDouble(getValue(index));
        }
        bool isInt(int index)
        {
            return JSONValue::isInt(getValue(index));
        }
        bool isIntType(int index)
        {
            return JSONValue::isIntType(getValue(index));
        }
        bool isLong(int index)
        {
            return JSONValue::isLong(getValue(index));
        }
        bool isLongType(int index)
        {
            return JSONValue::isLongType(getValue(index));
        }
        bool isBool(int index)
        {
            return JSONValue::isBool(getValue(index));
        }
        bool isArray(int index)
        {
            return JSONValue::isArray(getValue(index));
        }
        bool isObject(int index)
        {
            return JSONValue::isObject(getValue(index));
        }
        bool isNull(int index)
        {
            return JSONValue::isNull(getValue(index));
        }

        /// Lookup JSONString by array index. NULL if not present. Cast failure if not an Object.
        bool getString(int index,string &out)
        {
            return JSONValue::getString(getValue(index),out);
        }
        /// Lookup JSONBool by array index. NULL if not present. Cast failure if not an Object.
        bool getBool(int index,bool &out)
        {
            return JSONValue::getBool(getValue(index),out);
        }
        /// Lookup JSONNumber by array index. NULL if not present. Cast failure if not an Object.
        bool getDouble(int index,double &out)
        {
            return JSONValue::getDouble(getValue(index),out);
        }
        /// Lookup JSONNumber by array index. NULL if not present. Cast failure if not an Object.
        bool getLong(int index,long &out)
        {
            return JSONValue::getLong(getValue(index),out);
        }
        bool getLongType(int index,long &out)
        {
            return JSONValue::getLongType(getValue(index),out);
        }
        /// Lookup JSONNumber by array index. NULL if not present. Cast failure if not an Object.
        bool getInt(int index,int &out)
        {
            return JSONValue::getInt(getValue(index),out);
        }
        bool getIntType(int index,int &out)
        {
            return JSONValue::getIntType(getValue(index),out);
        }

        /// Lookup array child by index, NULL if not present. Cast failure if not an Array.
        JSONArray *getArray(int index) 
        {
            return getValue(index);
        }
        
        /// Lookup object child by index, NULL if not present. Cast failure if not an Array.
        JSONObject *getObject(int index) 
        {
            return getValue(index);
        }
        /// The following method allows any type to be returned. Use this when parsing unpredictable data
        JSONValue *getValue(int index) 
        {
            if(index<0 || index>=_size)
            {
               return NULL;
            }
            else
            {
               return _array[index];
            }
        }

        /// Used by the Parser when building the array
        bool put(int index, JSONValue *v)
        {
            if (index >= _size) {
                int oldSize = _size;
                int newSize = ArrayResize(_array, index+1, (index+(1+3)) / 4);
                if (newSize <= index) return false;
                _size = newSize;

                // initialise
                for(int i = oldSize ; i< newSize ; i++ ) _array[i] = NULL;
            }
            // Delete old entry if any
            if (_array[index] != NULL) delete _array[index];

            //set new entry
            _array[index] = v;

            return true;
        }

        string toString() {
           string s = "[";
           if (_size > 0) {
               s = s + _array[0].toString();
               for(int i = 1 ; i< _size ; i++ ) {
                  s = s + "," + _array[i].toString();
               }
           }
           s = s + "]";
           return s; 
        }

        int size() {
            return _size;
        }
};



/// Parse JSON text using a simple recursive descent parser
/// Exmaple
/// 
/// <pre>
///    string s = "{ \"firstName\": \"John\","+
///       " \"lastName\": \"Smith\","+
///       " \"age\": 25,"+
///       " \"address\": { \"streetAddress\": \"21 2nd Street\", \"city\": \"New York\", \"state\": \"NY\", \"postalCode\": \"10021\" },"+
///       " \"phoneNumber\": [ { \"type\": \"home\", \"number\": \"212 555-1234\" }, { \"type\": \"fax\", \"number\": \"646 555-4567\" } ],"+
///       " \"gender\":{ \"type\":\"male\" }  }";
///
///    JSONParser *parser = new JSONParser();
///
///    JSONValue *jv = parser.parse(s);
///
///    if (jv == NULL) {
///
///        Print("error:"+(string)parser.getErrorCode()+parser.getErrorMessage());
///
///    } else {
///
///        Print("PARSED:"+jv.toString());
///
///        if (jv.isObject()) {
///
///            JSONObject *jo = jv;
///
///            // Direct access - will throw null pointer if wrong getter used.
///
///            Print("firstName:" + jo.getString("firstName"));
///            Print("city:" + jo.getObject("address").getString("city"));
///            Print("phone:" + jo.getArray("phoneNumber").getObject(0).getString("number"));
///
///            // Safe access in case JSON data is missing or different.
///
///            if (jo.getString("firstName",s) ) Print("firstName = "+s);
///
///            // Loop over object returning JSONValue
///
///            JSONIterator *it = new JSONIterator(jo);
///            for( ; it.isValid() ; it.next()) {
///                Print("loop:"+it.key()+" = "+it.val().toString());
///            }
///            delete it;
///        }
///        delete jv;
///    }
///    delete parser;
/// </pre>

class JSONParser {
    private:
        /// Current parse position
        int _pos;
        /// The input string is expanded into an array of ushort (wchar)
        ushort _in[];
        /// Length of string
        int _len;
        /// The original input string
        string _instr;
        int _tab_size;

        int _errCode;
        string _errMsg;
        int _line;
        int _column;

        int _begin_object_pos;
        int _begin_object_line;
        int _begin_object_column;

        void setError(int code=1,string msg="") {
            _errCode |= code;
            if(msg != "")
            {
               if (_errMsg == "") {
                   _errMsg = "JSONParser::Error " + msg;
               } else {
                   _errMsg = _errMsg + "\n" + msg;
               }
            }
        }
        
        /// Parse a JSON Object
        JSONObject *parseObject() 
        {
            JSONObject *o = new JSONObject(_pos, _line, _column);
            skipSpace();
            if (expect('{')) {
                    while (_errCode == 0) {
                        skipSpace();
                        if(_pos >= _len)
                        {
                            setError(5, "unexpected end of file while parsing a JSONObject");
                            break;
                        }
                        if (_in[_pos] != '"')
                        {
                           if(!expect('}')) {
                              setError(2,"expected \" or } ");
                           }
                           break;
                        }

                        // Read the key
                        string key = parseString();

                        if (_errCode != 0 || key == NULL)
                           break;

                        skipSpace();

                        if (!expect(':'))
                        {
                           setError(2, "expected : ");
                           break;
                        }

                        // read the value
                        JSONValue *v = parseValue();
                        if (_errCode != 0 ) break;

                        o.put(key,v);

                        skipSpace();

                        if (!expectOptional(','))
                        {
                           if(!expect('}')) {
                              setError(2,"expected , or } ");
                           }
                           break;
                        }
                    }
            }
            if (_errCode != 0) {
                delete o;
                o = NULL;
            }
            return o;
        }

        bool isDigit0to9(ushort c) {
            return (c >= '0' && c <= '9' ); 
        }

        void skipSpace() {            
            for( ; _pos<_len ; _pos++)
            {
                ushort c = _in[_pos];
                if(c == '\n')
                {
                    _line++;
                    _column = 1;
                }
                else if(c == ' ')
                {
                    _column++;
                }
                else if(c == '\t')
                {
                    _column += _tab_size - ((_column-1) % _tab_size);
                }
                else if(c != '\r')
                {
                    break;
                }
            }
        }

        bool expect(ushort c)
        {
            if(_pos >= _len)
            {
                setError(1, "expected " +
                         ShortToString(c) + " (" + (string)c + ")" +
                         " got end of file");
                return false;
            }
            else if (c == _in[_pos]) {
                _pos++;
                _column++;
                return true;
            } else {
                setError(1, "");
                return false;
            }
        }

        bool expectOptional(ushort c)
        {
            bool ret=false;
            if (_pos < _len && c == _in[_pos]) {
                _pos++;
                _column++;
                ret = true;
            }
            return ret;
        }

        string parseString()
        {
            _begin_object_pos = _pos;
            _begin_object_line = _line;
            _begin_object_column = _column;
            string ret = "";
            if(expect('"')) {
                while(true) {
                    int end=_pos;
                    ushort c;
                    for( ; end < _len && (c=_in[end]) != '"' && c != '\\' ; end++)
                    {
#ifdef TOLERATE_TAB_CR_NL_IN_STRINGS
                        // According to the JSON standard, control characters like \r, \n, \t are not allowed inside
                        // strings. But if TOLERATE_TAB_CR_NL_IN_STRINGS is #defined, they are tolerated rather than raising an error.
                        if(c == '\n')
                        {
                            _line++;
                            _column = 1;
                        }
                        else if(c == '\t')
                        {
                            _column += _tab_size - ((_column-1) % _tab_size);
                        }
                        else if(c != '\r')
                        {
                            _column++;
                        }
#else
                        if(c < 0x20)
                        {
                           setError(4,"Control characters not allowed in JSON strings. Found character with decimal ASCII code " + (string)c + " .");
                           _pos = end;
                           break;
                        }
                        else
                        {
                           _column++;
                        }
#endif
                    }
                    if(_errCode != 0) break;

                    if (end >= _len) {
                        setError(5, "unexpected end of file while parsing a string");
                        _pos = end;
                        break;
                    }
                    // Check if character was escaped.
                    // TODO \" \\ \/ \b \f \n \r \t \u0000
                    if (c == '\\') {
                        // Add partial string and get more
                        if(end>_pos)
                        {
                           // If end==_pos, the following statement must not be executed because
                           // StringSubstr() would not insert an empty string - as one could believe.
                           // Instead, StringSubstr() would insert the whole rest part of _instr beginning from
                           // the position _pos.
                           ret = ret + StringSubstr(_instr,_pos,end-_pos);
                        }
                        end++;
                        _column++;
                        if (end >= _len) {
                          _pos = end;
                          setError(5, "unexpected end of file after escape \\ inside of string");
                          break;
                        } else {
                            c = 0;
							       int nrdigit;
                            switch(_in[end]) {
                                case '"':
                                case '\\':
                                case '/':
                                    c = _in[end];
                                    break;
                                case 'b': c = 8; break; // backspace - 8
                                case 'f': c = 12; break; // form feed 12
                                case 'n': c = '\n'; break;
                                case 'r': c = '\r'; break;
                                case 't': c = '\t'; break;
                                case 'u':
                                    for(nrdigit=0, c=0; nrdigit<4; nrdigit++)
                                    {
                                       end++;
                                       _column++;
                                       if (end >= _len) {
                                          _pos = end;
                                          setError(5, "unexpected end of file after escape \\u inside of string");
                                          break;
                                       }                                      

                                       if(_in[end]>='0' && _in[end]<='9')
                                       {
                                          c = c*16 + (_in[end]-'0');
                                       }
                                       else if(_in[end]>='a' && _in[end]<='f')
                                       {
                                          c = c*16 + (_in[end]-'a' + 10);
                                       }
                                       else if(_in[end]>='A' && _in[end]<='F')
                                       {
                                          c = c*16 + (_in[end]-'A' + 10);
                                       }
                                       else
                                       {
                                          _pos = end;
                                          setError(4,"unicode escape must be followed by four hexadecimal digits");
                                          break;
                                       }
                                    }
                                    break;
                                default:
                                    _pos = end;
                                    setError(4,"unknown escape");
                                    break;
                            }
                            if (_errCode != 0) break;
                            ret = ret + ShortToString(c);
                            end++;
                            _column++;
                            _pos = end;
                        }
                    } else if (c == '"') {
                        // End of string
                        if(end>_pos)
                        {
                           // If end==_pos, the following statement must not be executed because
                           // StringSubstr() would not insert an empty string - as one could believe.
                           // Instead, StringSubstr() would insert the whole rest part of _instr beginning from
                           // the position _pos.
                           ret = ret + StringSubstr(_instr,_pos,end-_pos);
                        }
                        end++;
                        _column++;
                        _pos = end;
                        break;
                    }
                }
            }
            if (_errCode != 0) {
                ret = NULL;
            }
            return ret;
        }

        JSONValue *parseValue() 
        {
            JSONValue *ret = NULL;
            skipSpace();
            string s;

            if(_pos >= _len)
            {
                setError(5, "unexpected end of file while parsing a JSONValue");
                return NULL;
            }

            if (_in[_pos] == '[')  {

                ret = (JSONValue*)parseArray();

            } else if (_in[_pos] == '{')  {

                ret = (JSONValue*)parseObject();

            } else if (_in[_pos] == '"')  {

                s = parseString();
                ret = (JSONValue*)new JSONString(s, _begin_object_pos, _begin_object_line, _begin_object_column);

            } else if (isDigit0to9(_in[_pos]) || _in[_pos]=='+' || _in[_pos]=='-' || _in[_pos]=='.') {

                bool isDouble = false;
                long sign;
                int i;

                _begin_object_pos = _pos;
                _begin_object_line = _line;
                _begin_object_column = _column;

                if (_in[_pos] == '-') {
                    sign = -1;
                    _pos++;
                    _column++;
                } else if (_in[_pos] == '+') {
                    sign = 1;
                    _pos++;
                    _column++;
                } else {
                    sign = 1;
                }

                i=_pos;
                while(i < _len && isDigit0to9(_in[i])) {
                    i++;
                }
                if(i < _len && _in[i]=='.')
                {
                    isDouble = true;
                    while(++i < _len && isDigit0to9(_in[i]))
                        { }
                }
                if(i < _len && (_in[i]=='e' || _in[i]=='E'))
                {
                    isDouble = true;
                    if(++i < _len && (_in[i]=='+' || _in[i]=='-'))
                       i++;
                    if(i >= _len || !isDigit0to9(_in[i]))
                    {
                        setError(5, "error parsing a number");
                        return NULL;
                    }
                    while(++i < _len && isDigit0to9(_in[i]))
                        { }
                }
           
                s = StringSubstr(_instr,_pos,i-_pos);
                if(isDouble) {
                    double d = sign * StringToDouble(s);
                    ret = (JSONValue*)new JSONNumber(d, _begin_object_pos, _begin_object_line, _begin_object_column); // Create a Number as double only
                } else {
                    long l = sign * StringToInteger(s);
                    ret = (JSONValue*)new JSONNumber(l, _begin_object_pos, _begin_object_line, _begin_object_column); // Create a Number as a long
                }
                _column += i-_pos;
                _pos = i;

            } else if (_in[_pos] == 't' && StringSubstr(_instr,_pos,4) == "true")  {

                ret = (JSONValue*)new JSONBool(true, _pos, _line, _column);
                _pos += 4;

            } else if (_in[_pos] == 'f' && StringSubstr(_instr,_pos,5) == "false")  {

                ret = (JSONValue*)new JSONBool(false, _pos, _line, _column);
                _pos += 5;

            } else if (_in[_pos] == 'n' && StringSubstr(_instr,_pos,4) == "null")  {

                ret = (JSONValue*)new JSONNull(_pos, _line, _column);
                _pos += 4;

            } else {

                setError(3, "error parsing a JSONValue");

            }

            if (_errCode != 0 && ret != NULL ) {
                delete ret;
                ret = NULL;
            }
            return ret;
        }

        JSONArray *parseArray()
        {
            JSONArray *ret = new JSONArray(_pos, _line, _column);

            int index = 0;
            skipSpace();
            if (expect('[')) {
                skipSpace();
                if (_pos >= _len) {
                    setError(5, "unexpected end of file while parsing a JSONArray");
                    delete ret;
                    return NULL;
                }
                if(!expectOptional(']')) {
                    while (_errCode == 0) {

                        // read the value
                        JSONValue *v = parseValue();
                        if (_errCode != 0) break;

                        if (!ret.put(index++,v)) {
                            setError(3,"memory error adding "+(string)index);
                            break;
                        }

                        skipSpace();
                        if (!expectOptional(','))
                        {
                           if(!expect(']')) {
                               setError(2,"JSONArray: expected , or ] ");
                           }
                           break;
                        }
                        skipSpace();
                    }
                }
            }

            if (_errCode != 0 ) {
                delete ret;
                ret = NULL;
            }
            return ret;
        }
    public:
        int getErrorCode()
        {
            return _errCode;
        }
        string getErrorMessage()
        {
            return _errMsg;
        }
        int get_char_position()
        {
            return _pos;
        }
        int get_line()
        {
            return _line;
        }
        int get_column()
        {
            return _column;
        }
        JSONParser(int tab_size=3)
        {  // tab_size is only used for counting columns when there is a TAB in the JSON code.
           // The column number can be read out with get_column() in case of an error (when parse() returns NULL).
           _tab_size = tab_size;
        }
        /// Parse a sequnce of characters and return a JSONValue.
        JSONValue *parse(
                string s ///< Serialized JSON text
             )
        {
            int inLen;
            JSONValue *ret = NULL;

            _instr = s;
            _len = StringToShortArray(_instr,_in); // nul '0' is added to length
            _pos = 0;
            _line = 1;
            _column = 1;
            _errCode = 0;
            _errMsg = "";
            inLen = StringLen(_instr);
            if (_len != inLen + 1 /* nul */ ) {
                setError(1, "unable to create array " + (string)inLen + " got " + (string)_len);
            } else {
                _len --;
                ret = parseValue();
                if (_errCode != 0) {
                    _errMsg = _errMsg + " in line " + (string)_line + " column " + (string)_column + " [" + StringSubstr(_instr,_pos,10) + "...]";
                }
            }
            return ret;
        }

};

/// Class to iterate over a JSONObject (not a JSONArray)
class JSONIterator {
    private:
        HashLoop * _l;

    public:
    // Create iterator and move to first item
    JSONIterator(JSONObject *jo) 
    {
        _l = new HashLoop(jo.getHash());
    }
    ~JSONIterator() 
    {
        delete _l;
    }

    // Check if current item is valid and so the function
    // val() will not return NULL.
    bool isValid() 
    {
        return _l.isValid();
    }

    // Check if current item is valid and so the function
    // val() will not return NULL.
    // Deprecated since the name hasNext is misleading
    bool hasNext()
    {
        return _l.isValid();
    }

    // Move to next item
    void next() {
        _l.next();
    }

    // Return item
    JSONValue *val()
    {
        return (JSONValue *) (_l.val());
    }

    // Return key
    string key()
    {
        return _l.key();
    }

};

/*
void json_demo() 
{
    string s = "{ \"firstName\": \"John\","+
       " \"lastName\": \"Smith\","+
       " \"age\": 25,"+
       " \"address\": { \"streetAddress\": \"21 2nd Street\", \"city\": \"New York\", \"state\": \"NY\", \"postalCode\": \"10021\" },"+
       " \"phoneNumber\": [ { \"type\": \"home\", \"number\": \"212 555-1234\" }, { \"type\": \"fax\", \"number\": \"646 555-4567\" } ],"+
       " \"gender\":{ \"type\":\"male\" }  }";

    JSONParser *parser = new JSONParser();
    JSONValue *jv = parser.parse(s);
    Print("json:");
    if (jv == NULL) {
        Print("error:"+(string)parser.getErrorCode()+parser.getErrorMessage());
    } else {
        Print("PARSED:"+jv.toString());
        if (jv.isObject()) {
            JSONObject *jo = jv;

            // Direct access - will throw null pointer if wrong getter used.
            Print("firstName:" + jo.getString("firstName"));
            Print("city:" + jo.getObject("address").getString("city"));
            Print("phone:" + jo.getArray("phoneNumber").getObject(0).getString("number"));

            // Safe access in case JSON data is missing or different.
            if (jo.getString("firstName",s) ) Print("firstName = "+s);

            // Loop over object returning JSONValue
            JSONIterator *it = new JSONIterator(jo);
            for( ; it.isValid() ; it.next()) {
                Print("loop:"+it.key()+" = "+it.val().toString());
            }
            delete it;
        }
        delete jv;
    }
    delete parser;
}
*/


#endif
