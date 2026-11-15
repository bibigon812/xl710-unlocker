#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/sockios.h>
#include <stdint.h>
#include "syscalls.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <interface_name>\n", argv[0]);
        return 1;
    }
    const char *ifname = argv[1];
    const uint32_t devid = 0x1572;
    const uint32_t mod = 0;

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) { perror("socket"); return 1; }

    struct ethtool_eeprom *eeprom = calloc(1, sizeof(*eeprom) + 2);
    if (!eeprom) { perror("calloc"); close(fd); return 1; }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
    ifr.ifr_data = (void *)eeprom;

    #define I40E_MAGIC ((devid << 16) | (I40E_NVM_SA << I40E_NVM_TRANS_SHIFT) | mod)

    // 1. Get EMP SR offset from word 0x48
    eeprom->cmd = ETHTOOL_GEEPROM;
    eeprom->magic = I40E_MAGIC;
    eeprom->len = 2;
    eeprom->offset = 0x48 << 1;
    if (ioctl(fd, SIOCETHTOOL, &ifr) < 0) {
        perror("ioctl reading EMP SR offset");
        goto cleanup;
    }
    uint16_t emp_offset = *(uint16_t *)(eeprom + 1);
    printf("EMP SR offset: 0x%04x\n", emp_offset);

    // 2. Get PHY capabilities offset from (emp_offset + 0x19)
    eeprom->offset = (emp_offset + 0x19) << 1;
    if (ioctl(fd, SIOCETHTOOL, &ifr) < 0) {
        perror("ioctl reading PHY offset pointer");
        goto cleanup;
    }
    uint16_t phy_offset = *(uint16_t *)(eeprom + 1) + emp_offset + 0x19;
    printf("PHY offset: 0x%04x\n", phy_offset);

    // 3. Get PHY data struct size from phy_offset
    eeprom->offset = phy_offset << 1;
    if (ioctl(fd, SIOCETHTOOL, &ifr) < 0) {
        perror("ioctl reading struct size");
        goto cleanup;
    }
    uint16_t struct_size = *(uint16_t *)(eeprom + 1);
    printf("PHY struct size: 0x%04x\n\n", struct_size);

    uint16_t misc_offset = 0x8;
    uint32_t byte_offsets[4];

    printf("=== Inspecting Structure States ===\n");
    for (int i = 0; i < 4; i++) {
        uint32_t word_offset = phy_offset + misc_offset + (struct_size + 1) * i;
        byte_offsets[i] = word_offset << 1;

        eeprom->cmd = ETHTOOL_GEEPROM;
        eeprom->magic = I40E_MAGIC;
        eeprom->len = 2;
        eeprom->offset = byte_offsets[i];
        if (ioctl(fd, SIOCETHTOOL, &ifr) < 0) {
            perror("ioctl reading struct");
            continue;
        }
        uint16_t val = *(uint16_t *)(eeprom + 1);
        printf("Struct %d (word 0x%04x / byte 0x%04x): 0x%04x %s\n",
               i, word_offset, byte_offsets[i], val,
               (val & 0x0800) ? "<- locked" : "<- unlocked");
    }

    printf("\n=== Unlocking Physical Ports (Structs 0 & 1 -> 0x630c) ===\n");
    for (int i = 0; i < 2; i++) {
        eeprom->cmd = ETHTOOL_SEEPROM;
        eeprom->magic = I40E_MAGIC;
        eeprom->len = 2;
        eeprom->offset = byte_offsets[i];
        *(uint16_t *)(eeprom + 1) = 0x630c;
        if (ioctl(fd, SIOCETHTOOL, &ifr) < 0) {
            perror("ioctl write");
        } else {
            printf("Struct %d (byte 0x%04x) written to 0x630c [OK]\n", i, byte_offsets[i]);
        }
        sleep(1);
    }

    printf("\n=== Recalculating NVM Checksum ===\n");
    eeprom->cmd = ETHTOOL_SEEPROM;
    eeprom->magic = (devid << 16) | ((I40E_NVM_CSUM | I40E_NVM_SA) << I40E_NVM_TRANS_SHIFT) | mod;
    eeprom->len = 2;
    eeprom->offset = 0;
    if (ioctl(fd, SIOCETHTOOL, &ifr) < 0) {
        perror("ioctl checksum");
    } else {
        printf("NVM Checksum updated successfully.\n");
    }

cleanup:
    free(eeprom);
    close(fd);
    return 0;
}