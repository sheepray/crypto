
#include "aes128gcm.h"



/* Under the 16-byte (128-bit) key "k",
 and the 12-byte (96-bit) initial value "IV",
 encrypt the plaintext "plaintext" and store it at "ciphertext".
 The length of the plaintext is a multiple of 16-byte (128-bit) given by len_p (e.g., len_p = 2 for a 32-byte plaintext).
 The length of the ciphertext "ciphertext" is len_p*16 bytes.
 The authentication tag is obtained by the 16-byte tag "tag".
 For the authentication an additional data "add_data" can be added.
 The number of blocks for this additional data is "len_ad" (e.g., len_ad = 1 for a 16-byte additional data).
 */


/*test code;*/
void PrintfBitString(unsigned char *bitString, int length){
    printf("\n");
    for(int i=0; i< length; i++){
        printf("%x ", bitString[i]);
    }
    printf("\n");
}


/* Initialize J0 */
void InitializeJZ(unsigned char JZ[16], const unsigned char *IV){
    int i=0;
    for(;i<12; i++){
        JZ[i] = IV[i];
    }
    
    for(;i<15; i++){
        JZ[i] = 0;
    }
    
    JZ[i] = 1;
}


/* store $length of most significant bits of X to _MSB */
void MSB(unsigned char *X, unsigned char *_MSB, int length){
    /* initialize _MSB */
    memset(_MSB, 0, 16*sizeof(unsigned char));
    
    if(length == 0){
        return;
    }
    
    int i;
    for(i=0; i<length/8; i++){
        _MSB[i] = X[i];
    }
    
    unsigned char tempChar = 1;
    
    if(length % 8 != 0){
        tempChar = ~( (tempChar<<(8 - length%8)) -1);
        _MSB[i] = X[i] & tempChar;
    }
}

/* Fetch $length of least significant bits of X to _LSB */
void LSB(unsigned char *X, unsigned char *_LSB, int length){
    /* initialize _LSB */
    memset(_LSB, 0, sizeof(unsigned char));
    
    if(length == 0){
        return;
    }
    
    int i;
    if(length%8 == 0){
        for(i=0; i<length/8; i++){
            _LSB[i] = X[16 - length/8 + i];
        }
    }else{
        for(i=0; i<length/8; i++){
            _LSB[i+1] = X[16 - length/8 + i];
        }
        
        unsigned char tempChar = 1;
        _LSB[0] = X[15 - length/8] & ((tempChar << (length % 8)) - 1);
    }
}


/* Increment X */
void INC(unsigned char *X, int s, int length_of_X){
    unsigned char _MSB[16];
    unsigned char _LSB[16];
    MSB(X, _MSB, length_of_X-s);
    LSB(X, _LSB, s);
    
    
    int flagForPlusOne = 1;
    _LSB[(s-1)/8] += 1;
    /* check out carry and _LSB module 2^32 */
    for(int i=((s-1)/8); i>0 && (flagForPlusOne == 1); i--){
        if(_LSB[i] == 0){
            _LSB[i-1] += 1;
        }else{
            flagForPlusOne = 0;
        }
    }
    
    /* combine MSB and LSB */
    if(s%8 == 0){
        int i=0;
        for(;i<(length_of_X -s)/8; i++){
            X[i] = _MSB[i];
        }
        for(int j=0; j<s/8; i++, j++){
            X[i] = _LSB[j];
        }
        
    }else{
        printf("INC: case for s module 8 != 0\n");
    }
}


/* GCTR function */
void GCTR(const unsigned char *key, unsigned char *ICB, const unsigned char *X, unsigned char *Y, int length_of_X){
    int n = length_of_X/128;
    if(length_of_X % 128 != 0){
        n += 1;
        printf("GCTR: implementation of step 7\n");
    }
    
    unsigned char CB[n][16];
    unsigned char Y_delegation[n][16];
    unsigned char X_delegation[n][16];
    
    /* create Xi */
    for(int j=0; j<length_of_X/8; j++){
        X_delegation[j/16][j%16] = X[j];
    }
    
    
    for(int i=0; i<16; i++){
        CB[0][i] = ICB[i];
    }
    
    for(int j=1; j<n; j++){
        /* copy INC32(CBi-1) to CBi*/
        unsigned char tempChar[16];
        for(int k=0; k<16 ; k++){
            tempChar[k] = CB[j-1][k];
        }
        
        INC(tempChar, 32, 128);
        
        for(int k=0; k<16; k++){
            CB[j][k] = tempChar[k];
        }
        
    }
    
    for(int j=0; j<n; j++){
        /* Xi XOR CIPH(CBi) */
        unsigned char tempCiphertext[16];
        aes128e(tempCiphertext, CB[j], key);
        
        for(int k=0; k<16; k++){
            Y_delegation[j][k] = tempCiphertext[k]^X_delegation[j][k];
        }
    }
    
    /* combine Y_delegation into Y */
    int count=0;
    for(int j=0; j<n; j++){
        for(int k=0; k<16; k++){
            Y[count] = Y_delegation[j][k];
            count++;
        }
    }
    
}


/* Return the $index bit of X */
int indexOfArray(unsigned char *X, int index){
    unsigned char X_delegation[16];
    for(int i=0; i<16; i++){
        X_delegation[i] = X[i];
    }
    
    unsigned char choose_value = X_delegation[index/8];
    return ( choose_value>>(7-(index%8)) ) & 1;
}


/* Shift X to right by one */
void ShiftRightByOne(unsigned char *X){
    for(int i=15; i>=0; i--){
        X[i] = X[i] >> 1;
        if(i != 0 && (X[i-1]&1) == 1){
            X[i] = X[i] | 0x80;
        }
    }
}

/* Galois field multiplication of X and Y */
void XmY(unsigned char *X, unsigned char *Y, unsigned char *Z){
    /* initialize R */
    unsigned char R[16];
    unsigned char V[16];
    memset(R, 0, 16*sizeof(unsigned char));
    R[0] = 0xe1;
    
    memset(Z, 0, 16*sizeof(unsigned char));
    
    for(int i=0; i<16; i++){
        V[i] = Y[i];
    }
    
  
    for(int i=0; i<128; i++){
        if(indexOfArray(X, i) == 1){
            /* Z(i+1) = Z(i) xor Vi */
            for(int j=0; j<16; j++){
                Z[j] = Z[j]^V[j];
            }
        }
        
      
        if((V[15]&1) == 1){
            /* calculate V(i+1) */
            ShiftRightByOne(V);

            /* V XOR R */
            for(int k=0; k<16; k++){
                V[k] = V[k] ^ R[k];
            }
        }else{
            /* calculate V(i+1) */
            ShiftRightByOne(V);
        }
        
    }
}


/* Ghash hash function */
void GHASH(unsigned char *X, const unsigned long length_of_X, unsigned char *H, unsigned char *result){
    if(length_of_X % 128 != 0){
        printf("GHASH: wrong length variable\n");
        return;
    }
    
    int m = length_of_X/128;
    
    
//    printf("%d\n", m);
    
    unsigned char X_delegation[m][16];
    unsigned char Y_delegation[16];
    
    /* create Xi */
    for(int j=0; j<length_of_X/8; j++){
        X_delegation[j/16][j%16] = X[j];
    }
    
    /* set empty */
    memset(Y_delegation, 0, 16*sizeof(unsigned char));
    
    for(int i=0; i<m; i++){
        unsigned char temp_result[16];
        for(int j=0; j<16; j++){
            temp_result[j] = (Y_delegation[j]^X_delegation[i][j]);
        }
        
        XmY(temp_result, H, Y_delegation);
        
    }
    
    /* copy Y to result */
    for(int i=0; i<16; i++){
        result[i] = Y_delegation[i];
    }
}


void aes128gcm(unsigned char *ciphertext, unsigned char *tag, const unsigned char *k, const unsigned char *IV, const unsigned char *plaintext, const unsigned long len_p, const unsigned char* add_data, const unsigned long len_ad) {
    
    unsigned char H[16];
    unsigned char zero_array[16];
    unsigned char C[3*16];
    memset(zero_array, 0, 16*sizeof(unsigned char));
    aes128e(H, zero_array, k);
    
    unsigned char JZ[16];
    unsigned char JZ_ini[16];
    InitializeJZ(JZ, IV);
    memcpy(JZ_ini, JZ, 16);
    INC(JZ, 32, 128);
    
    GCTR(k, JZ, plaintext, ciphertext, len_p*128);
    
    
    /* implement S = GHASHH (A || 0v || C || 0u || [len(A)]64 || [len(C)]64). */
    unsigned char pre_ghash_value[(len_ad+len_p+1)*16];
    /* copy add_data */
    int i = 0;
    for(; i<len_ad*16; i++){
        pre_ghash_value[i] = add_data[i];
    }
    
    /* copy Cipertext */
    for(int j=0; j<len_p*16; j++){
        pre_ghash_value[i] = ciphertext[j];
        i++;
    }
    
    unsigned char A_for_hash[8], C_for_hash[8];
    /* create len(A) and len(C) */
    memset(A_for_hash, 0, 8*sizeof(unsigned char));
    memset(C_for_hash, 0, 8*sizeof(unsigned char));
    unsigned long len_ad_with_128 = len_ad*128;
    unsigned long len_p_with_128 = len_p*128;
    memcpy(A_for_hash, &len_ad_with_128, 8);
    memcpy(C_for_hash, &len_p_with_128, 8);

    
    /* Add A_for_hash and C_for_hash to pre_ghash_value */
    for(int j=7; j>=0; j--){
        pre_ghash_value[i] = A_for_hash[j];
        i++;
    }
    for(int j=7; j>=0; j--){
        pre_ghash_value[i] = C_for_hash[j];
        i++;
    }
    
    unsigned char GHASH_result[16];
    GHASH(pre_ghash_value, (len_ad + len_p + 1)*128, H, GHASH_result);
    GCTR(k, JZ_ini, GHASH_result, tag, (len_ad + len_p + 1)*128);
    
}


