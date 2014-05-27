/*
 * Utility program to generate kermit script for loading DaVinci
 * Initial Bootloader over serial port.
 *
 * Copyright (c) 2007 Sergey Kubushin <ksi@koi8.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define MAX_LDR_SIZE	(0x3800)

const char *ksc_head = \
"def put_ldr_hdr {\n" \
"\tout \"    ACK\"\n" \
"\tout \\n\n" \
"\tout ";

const char *ksc_crc = \
"\n}\n\n" \
"def put_crc32 {\n" \
"\tout 0000000077073096ee0e612c990951ba076dc419706af48fe963a5359e6495a3\n" \
"\tout 0edb883279dcb8a4e0d5e91e97d2d98809b64c2b7eb17cbde7b82d0790bf1d91\n" \
"\tout 1db710646ab020f2f3b9714884be41de1adad47d6ddde4ebf4d4b55183d385c7\n" \
"\tout 136c9856646ba8c0fd62f97a8a65c9ec14015c4f63066cd9fa0f3d638d080df5\n" \
"\tout 3b6e20c84c69105ed56041e4a26771723c03e4d14b04d447d20d85fda50ab56b\n" \
"\tout 35b5a8fa42b2986cdbbbc9d6acbcf94032d86ce345df5c75dcd60dcfabd13d59\n" \
"\tout 26d930ac51de003ac8d75180bfd0611621b4f4b556b3c423cfba9599b8bda50f\n" \
"\tout 2802b89e5f058808c60cd9b2b10be9242f6f7c8758684c11c1611dabb6662d3d\n" \
"\tout 76dc419001db710698d220bcefd5102a71b1858906b6b51f9fbfe4a5e8b8d433\n" \
"\tout 7807c9a20f00f9349609a88ee10e98187f6a0dbb086d3d2d91646c97e6635c01\n" \
"\tout 6b6b51f41c6c6162856530d8f262004e6c0695ed1b01a57b8208f4c1f50fc457\n" \
"\tout 65b0d9c612b7e9508bbeb8eafcb9887c62dd1ddf15da2d498cd37cf3fbd44c65\n" \
"\tout 4db261583ab551cea3bc0074d4bb30e24adfa5413dd895d7a4d1c46dd3d6f4fb\n" \
"\tout 4369e96a346ed9fcad678846da60b8d044042d7333031de5aa0a4c5fdd0d7cc9\n" \
"\tout 5005713c270241aabe0b1010c90c20865768b525206f85b3b966d409ce61e49f\n" \
"\tout 5edef90e29d9c998b0d09822c7d7a8b459b33d172eb40d81b7bd5c3bc0ba6cad\n" \
"\tout edb883209abfb3b603b6e20c74b1d29aead547399dd277af04db261573dc1683\n" \
"\tout e3630b1294643b840d6d6a3e7a6a5aa8e40ecf0b9309ff9d0a00ae277d079eb1\n" \
"\tout f00f93448708a3d21e01f2686906c2fef762575d806567cb196c36716e6b06e7\n" \
"\tout fed41b7689d32be010da7a5a67dd4accf9b9df6f8ebeeff917b7be4360b08ed5\n" \
"\tout d6d6a3e8a1d1937e38d8c2c44fdff252d1bb67f1a6bc57673fb506dd48b2364b\n" \
"\tout d80d2bdaaf0a1b4c36034af641047a60df60efc3a867df55316e8eef4669be79\n" \
"\tout cb61b38cbc66831a256fd2a05268e236cc0c7795bb0b4703220216b95505262f\n" \
"\tout c5ba3bbeb2bd0b282bb45a925cb36a04c2d7ffa7b5d0cf312cd99e8b5bdeae1d\n" \
"\tout 9b64c2b0ec63f226756aa39c026d930a9c0906a9eb0e363f7207678505005713\n" \
"\tout 95bf4a82e2b87a147bb12bae0cb61b3892d28e9be5d5be0d7cdcefb70bdbdf21\n" \
"\tout 86d3d2d4f1d4e24268ddb3f81fda836e81be16cdf6b9265b6fb077e118b74777\n" \
"\tout 88085ae6ff0f6a7066063bca11010b5c8f659efff862ae69616bffd3166ccf45\n" \
"\tout a00ae278d70dd2ee4e0483543903b3c2a7672661d06016f74969474d3e6e77db\n" \
"\tout aed16a4ad9d65adc40df0b6637d83bf0a9bcae53debb9ec547b2cf7f30b5ffe9\n" \
"\tout bdbdf21ccabac28a53b3933024b4a3a6bad03605cdd7069354de572923d967bf\n" \
"\tout b3667a2ec4614ab85d681b022a6f2b94b40bbe37c30c8ea15a05df1b2d02ef8d";

const char *part_str = "\n}\n\ndef put_ldr_part%d {";

const unsigned int crc32_table[256] = {
	0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
	0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
	0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
	0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
	0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
	0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
	0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
	0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
	0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
	0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
	0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
	0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
	0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
	0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
	0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
	0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
	0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
	0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
	0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
	0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
	0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
	0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
	0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
	0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
	0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
	0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
	0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
	0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
	0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
	0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
	0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
	0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
	0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
	0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
	0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
	0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
	0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
	0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
	0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
	0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
	0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
	0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
	0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
	0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
	0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
	0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
	0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
	0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
	0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
	0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
	0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
	0x2d02ef8dL
};

void usage(char *progname)
{
	fprintf(stderr, "\nUsage: %s input_file output_file\n", progname);
}

int main(int argc, char **argv)
{
	int		count, fd, i, j, k;
	char		out_str[128];
	char		out_part[64];
	struct stat	st;
	unsigned char	*ptr, ldr[MAX_LDR_SIZE];	
	unsigned int	crc32;

	if (argc < 3) {
		usage(argv[0]);
		return(-1);
	}

	if (stat(argv[1], &st))	{
		perror(argv[1]);
		return(-1);
	}
	
	/* Skip first 0x20 bytes */
	st.st_size -= 0x20;

	if (st.st_size > MAX_LDR_SIZE) {
		fprintf(stderr, "\nInput file %s too big (%d bytes)\n", argv[1], st.st_size);
		return(-1);
	}

	if ((fd = open(argv[1], O_RDONLY)) < 0) {
		perror(argv[1]);
		return(-1);
	}

	/* Skip first 0x20 bytes */
	if (lseek(fd, 0x20, SEEK_SET) != 0x20) {
		perror(argv[1]);
		return(-1);
	}

	if ((count = read(fd, &ldr[0], st.st_size)) != st.st_size) {
		perror(argv[1]);
		return(-1);
	}

	close(fd);

	ptr = &ldr[0];

	/* Calculate CRC32 checksum */
	crc32 = 0xffffffff;
	while (count-- > 0)
		crc32 = crc32_table[(crc32 ^ *ptr++) & 0xff] ^ (crc32 >> 8);

	/* Generate output and write it to output file */
	if ((fd = creat(argv[2], S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH)) < 0) {
		perror(argv[2]);
		return(-1);
	}

	count = strlen(ksc_head);
	if (write(fd, ksc_head, count) != count) {
		perror(argv[2]);
		close(fd);
		if (unlink(argv[2]))
			perror(argv[2]);
		return(-1);
	}

	snprintf(out_str, 21, "%08x%04x01000000", crc32, st.st_size);

	if (write(fd, out_str, 20) != 20) {
		perror(argv[2]);
		close(fd);
		if (unlink(argv[2]))
			perror(argv[2]);
		return(-1);
	}

	count = strlen(ksc_crc);
	if (write(fd, ksc_crc, count) != count) {
		perror(argv[2]);
		close(fd);
		if (unlink(argv[2]))
			perror(argv[2]);
		return(-1);
	}

	count = 0;
	j = k = 0;
	ptr = out_str;
	for (i = 0; i < (st.st_size / 4); i++) {

		if ((i % 8) == 0) {
			if (count != 0) {
				if ((j % 128) == 0) {
					snprintf(out_part, 24, part_str, k++);
					if (write(fd, out_part, 23) != 23) {
						perror(argv[2]);
						close(fd);
						if (unlink(argv[2]))
							perror(argv[2]);
						return(-1);
					}
				}
				if (write(fd, out_str, count) != count) {
					perror(argv[2]);
					close(fd);
					if (unlink(argv[2]))
						perror(argv[2]);
					return(-1);
				}
				j++;
			}
			ptr = out_str;
			snprintf(ptr, 7, "\n\tout ");
			ptr += 6;
			count = 6;
		}
		snprintf(ptr, 9, "%08x", *((unsigned int *)&ldr[0] + i));
		ptr += 8;
		count += 8;
	}

	snprintf(ptr, 4, "\n}\n");
	count += 3;
	if ((write(fd, out_str, count)) != count) {
		perror(argv[2]);
		close(fd);
		if (unlink(argv[2]))
			perror(argv[2]);
		return(-1);
	}

	snprintf(out_str, 19, "\ndef put_loader {\n");
	if ((write(fd, out_str, 18)) != 18) {
		perror(argv[2]);
		close(fd);
		if (unlink(argv[2]))
			perror(argv[2]);
		return(-1);
	}

	for (j = 0; j < k; j++) {
		snprintf(out_str, 16, "\tput_ldr_part%d\n", j);
		if ((write(fd, out_str, 15)) != 15) {
			perror(argv[2]);
			close(fd);
			if (unlink(argv[2]))
				perror(argv[2]);
			return(-1);
		}
	}
	
	snprintf(out_str, 3, "}\n");
	if ((write(fd, out_str, 2)) != 2) {
		perror(argv[2]);
		close(fd);
		if (unlink(argv[2]))
			perror(argv[2]);
		return(-1);
	}

	close(fd);
	return(0);
}

