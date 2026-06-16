#include <stdio.h>
#include "sha256.h"

static void print_hash(const uint8_t digest[SHA256_DIGEST_SIZE]) {
    int i;
    for (i = 0; i < SHA256_DIGEST_SIZE; i++)
        printf("%02x", digest[i]);
    printf("\n");
}

int main() {
    uint8_t digest[SHA256_DIGEST_SIZE];

    /* Test 1: "RedBlockBlue" */
    sha256((const uint8_t *)"The quick brown fox jumps over the lazy dodgfdfdfdfffffddddddddd", 64, digest);
    printf("got:      ");
    print_hash(digest);


    return 0;
}