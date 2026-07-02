//+------------------------------------------------------------------+
//|                                                      siphash.mqh |
//|                     Copyright 2022-2023, MegaConnector Software. |
//|                                      https://t.me/mega_connector |
//+------------------------------------------------------------------+
#property copyright "Copyright 2022-2023, MegaConnector Software."
#property link      "https://t.me/mega_connector"
#property version   "1.00"
#property strict

class SipHash {
private:
	uint c, d, index;
	ulong v0, v1, v2, v3, m;
	uchar input_len;

	inline const ulong read8(const char &p[], const uint o = 0) {
        return (((ulong)(p[0 + o])) | ((ulong)(p[1 + o]) << 8) |
       ((ulong)(p[2 + o]) << 16) | ((ulong)(p[3 + o]) << 24) |
       ((ulong)(p[4 + o]) << 32) | ((ulong)(p[5 + o]) << 40) |
       ((ulong)(p[6 + o]) << 48) | ((ulong)(p[7 + o]) << 56));
	}

    ulong rotate_left(
            const ulong val,
            const int cnt) {
        return (val << cnt) | (val >> (64 - cnt));
    }

	void compress() {
        v0 += v1;
        v2 += v3;
        v1 = rotate_left(v1, 13);
        v3 = rotate_left(v3, 16);
        v1 ^= v0;
        v3 ^= v2;
        v0 = rotate_left(v0, 32);
        v2 += v1;
        v0 += v3;
        v1 = rotate_left(v1, 17);
        v3 = rotate_left(v3, 21);
        v1 ^= v2;
        v3 ^= v0;
        v2 = rotate_left(v2, 32);
	}

	void digest_block() {
        v3 ^= m;
        for (uint i = 0; i < c; ++i) {
            compress();
        }
        v0 ^= m;
	}

public:

    SipHash() {};
    ~SipHash() {};

    /** \brief Initialize SipHash
     * SipHash-2-4 for best performance
     * SipHash-4-8 for conservative security
     * Siphash-1-3 for performance at the risk of yet-unknown DoS attacks
     * \param key   128-bit secret key
     * \param _c    Number of rounds per message block
     * \param _d    Number of finalization rounds
     */
    void init(
            const char &key[16],
            const uint _c = 2,
            const uint _d = 4) {
        this.c = _c;
        this.d = _d;

        const ulong k0 = read8(key);
        const ulong k1 = read8(key, 8);

        v0 = (0x736f6d6570736575 ^ k0);
        v1 = (0x646f72616e646f6d ^ k1);
        v2 = (0x6c7967656e657261 ^ k0);
        v3 = (0x7465646279746573 ^ k1);

        index = 0;
        input_len = 0;
        m = 0;
    }
    
    /** \brief Initialize SipHash
     * SipHash-2-4 for best performance
     * SipHash-4-8 for conservative security
     * Siphash-1-3 for performance at the risk of yet-unknown DoS attacks
     * \param key   128-bit secret key
     * \param _c    Number of rounds per message block
     * \param _d    Number of finalization rounds
     */
    SipHash(const char &key[16], const uint _c = 2, const uint _d = 4) {
        init(key, _c, _d);
    };


    void update(const char byte) {
        ++input_len;
        m |= (((ulong) byte & 0xff) << (index++ * 8));
        if (index >= 8) {
            digest_block();
            index = 0;
            m = 0;
        }
    };

    void update(const char &data[], const uint length) {
        for (uint i = 0; i < length; ++i) {
            update(data[i]);
        }
    }

    void update(const string &data) {
        if (StringLen(data) == 0) return;
        int bytes;
		uchar utf8_array[];
		bytes = StringToCharArray(data, utf8_array, 0, -1, CP_UTF8);
		bytes = ArraySize(utf8_array);
		bytes--;
        for (int i = 0; i < bytes; ++i) {
            update(utf8_array[i]);
        }
    }

    ulong digest() {
        while (index < 7) {
            m |= 0 << (index++ * 8);
        }
        m |= ((ulong) input_len) << (index * 8);
        digest_block();
        v2 ^= 0xff;
        for(uint i = 0; i < d; ++i){
            compress();
        }
        return ((ulong) v0 ^ v1 ^ v2 ^ v3);
    }
}; // SipHash


ulong siphash(
        const string data,
        const char &key[16],
        const uint c,
        const uint d) export {
    SipHash h(key, c, d);
    h.update(data);
    return h.digest();
}

ulong siphash_2_4(
        const string data,
        const char &key[16]) {
    SipHash h(key, 2, 4);
    h.update(data);
    return h.digest();
}

ulong siphash_4_8(
        const string data,
        const char &key[16]) {
    SipHash h(key, 4, 8);
    h.update(data);
    return h.digest();
}

const ulong siphash_1_3(
        const string data,
        const char &key[16]) {
    SipHash h(key, 1, 3);
    h.update(data);
    return h.digest();
}
