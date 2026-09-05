#include <errno.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

static int run_ioctl(int fd, struct ifreq *ifr, void *data)
{
	ifr->ifr_data = data;
	return ioctl(fd, SIOCETHTOOL, ifr);
}

static int get_settings(int fd, struct ifreq *ifr, struct ethtool_cmd *cmd)
{
	memset(cmd, 0, sizeof(*cmd));
	cmd->cmd = ETHTOOL_GSET;
	return run_ioctl(fd, ifr, cmd);
}

static void show_features(int fd, struct ifreq *ifr)
{
	struct ethtool_sset_info *sset;
	struct ethtool_gstrings *names;
	struct ethtool_gfeatures *features;
	unsigned int count;
	unsigned int blocks;

	sset = calloc(1, sizeof(*sset) + sizeof(sset->data[0]));
	if (!sset)
		return;
	sset->cmd = ETHTOOL_GSSET_INFO;
	sset->sset_mask = 1ULL << ETH_SS_FEATURES;
	if (run_ioctl(fd, ifr, sset) || !(sset->sset_mask &
					       (1ULL << ETH_SS_FEATURES))) {
		free(sset);
		return;
	}
	count = sset->data[0];
	free(sset);
	blocks = (count + 31) / 32;
	names = calloc(1, sizeof(*names) + count * ETH_GSTRING_LEN);
	features = calloc(1, sizeof(*features) + blocks *
			  sizeof(features->features[0]));
	if (!names || !features)
		goto out;
	names->cmd = ETHTOOL_GSTRINGS;
	names->string_set = ETH_SS_FEATURES;
	names->len = count;
	features->cmd = ETHTOOL_GFEATURES;
	features->size = blocks;
	if (run_ioctl(fd, ifr, names) || run_ioctl(fd, ifr, features))
		goto out;

	for (unsigned int i = 0; i < count; i++) {
		char name[ETH_GSTRING_LEN + 1];
		uint32_t bit = 1U << (i % 32);
		const struct ethtool_get_features_block *block =
			&features->features[i / 32];

		memcpy(name, names->data + i * ETH_GSTRING_LEN,
		       ETH_GSTRING_LEN);
		name[ETH_GSTRING_LEN] = '\0';
		if (!strstr(name, "checksum") && !strstr(name, "scatter") &&
		    !strstr(name, "segmentation") && !strstr(name, "receive") &&
		    !strstr(name, "rx-hashing"))
			continue;
		printf("FEATURE name=%s active=%u available=%u requested=%u fixed=%u\n",
		       name, !!(block->active & bit), !!(block->available & bit),
		       !!(block->requested & bit), !!(block->never_changed & bit));
	}
out:
	free(features);
	free(names);
}

static int show_stats(int fd, struct ifreq *ifr)
{
	struct ethtool_sset_info *sset;
	struct ethtool_gstrings *names;
	struct ethtool_stats *stats;
	unsigned int count;
	int ret = 1;

	sset = calloc(1, sizeof(*sset) + sizeof(sset->data[0]));
	if (!sset)
		return 1;
	sset->cmd = ETHTOOL_GSSET_INFO;
	sset->sset_mask = 1ULL << ETH_SS_STATS;
	if (run_ioctl(fd, ifr, sset) ||
	    !(sset->sset_mask & (1ULL << ETH_SS_STATS))) {
		perror("ETHTOOL_GSSET_INFO");
		goto out_sset;
	}
	count = sset->data[0];
	names = calloc(1, sizeof(*names) + count * ETH_GSTRING_LEN);
	stats = calloc(1, sizeof(*stats) + count * sizeof(stats->data[0]));
	if (!names || !stats)
		goto out;

	names->cmd = ETHTOOL_GSTRINGS;
	names->string_set = ETH_SS_STATS;
	names->len = count;
	stats->cmd = ETHTOOL_GSTATS;
	stats->n_stats = count;
	if (run_ioctl(fd, ifr, names) || run_ioctl(fd, ifr, stats)) {
		perror("ETHTOOL_GSTATS");
		goto out;
	}

	for (unsigned int i = 0; i < count; i++) {
		char name[ETH_GSTRING_LEN + 1];

		memcpy(name, names->data + i * ETH_GSTRING_LEN,
		       ETH_GSTRING_LEN);
		name[ETH_GSTRING_LEN] = '\0';
		printf("STAT name=%s value=%llu\n", name,
		       (unsigned long long)stats->data[i]);
	}
	ret = 0;
out:
	free(stats);
	free(names);
out_sset:
	free(sset);
	return ret;
}

static int show_regs(int fd, struct ifreq *ifr)
{
	struct ethtool_drvinfo info = { .cmd = ETHTOOL_GDRVINFO };
	struct ethtool_regs *regs;
	uint32_t *word;
	unsigned int count;
	int ret = 1;

	if (run_ioctl(fd, ifr, &info)) {
		perror("ETHTOOL_GDRVINFO");
		return 1;
	}
	regs = calloc(1, sizeof(*regs) + info.regdump_len);
	if (!regs)
		return 1;
	regs->cmd = ETHTOOL_GREGS;
	regs->len = info.regdump_len;
	if (run_ioctl(fd, ifr, regs)) {
		perror("ETHTOOL_GREGS");
		goto out;
	}

	word = (uint32_t *)regs->data;
	count = regs->len / sizeof(*word);
	printf("REGS version=%#x words=%u\n", regs->version, count);
	for (unsigned int i = 0; i < count; i++)
		printf("REG index=%u value=%08x\n", i, word[i]);
	ret = 0;
out:
	free(regs);
	return ret;
}

static int show(int fd, struct ifreq *ifr)
{
	struct ethtool_cmd settings;
	struct ethtool_value link = { .cmd = ETHTOOL_GLINK };
	struct ethtool_ringparam ring = { .cmd = ETHTOOL_GRINGPARAM };
	struct ethtool_pauseparam pause = { .cmd = ETHTOOL_GPAUSEPARAM };
	struct ethtool_eee eee = { .cmd = ETHTOOL_GEEE };
	int failed = 0;

	if (get_settings(fd, ifr, &settings)) {
		perror("ETHTOOL_GSET");
		failed = 1;
	} else {
		printf("LINK speed=%u duplex=%u autoneg=%u supported=%08x "
		       "advertising=%08x lp=%08x mdio=%u\n",
		       ethtool_cmd_speed(&settings), settings.duplex,
		       settings.autoneg, settings.supported, settings.advertising,
		       settings.lp_advertising, settings.mdio_support);
	}
	if (!run_ioctl(fd, ifr, &link))
		printf("CARRIER up=%u\n", link.data);
	else
		perror("ETHTOOL_GLINK");
	if (!run_ioctl(fd, ifr, &ring))
		printf("RING rx=%u/%u tx=%u/%u\n", ring.rx_pending,
		       ring.rx_max_pending, ring.tx_pending, ring.tx_max_pending);
	else
		perror("ETHTOOL_GRINGPARAM");
	if (!run_ioctl(fd, ifr, &pause))
		printf("PAUSE autoneg=%u rx=%u tx=%u\n", pause.autoneg,
		       pause.rx_pause, pause.tx_pause);
	else
		perror("ETHTOOL_GPAUSEPARAM");
	if (!run_ioctl(fd, ifr, &eee))
		printf("EEE enabled=%u active=%u tx_lpi=%u timer=%u "
		       "supported=%08x advertised=%08x lp=%08x\n",
		       eee.eee_enabled, eee.eee_active, eee.tx_lpi_enabled,
		       eee.tx_lpi_timer, eee.supported, eee.advertised,
		       eee.lp_advertised);
	else
		perror("ETHTOOL_GEEE");
	show_features(fd, ifr);
	return failed;
}

static int set_eee(int fd, struct ifreq *ifr, int enabled)
{
	struct ethtool_eee eee = { .cmd = ETHTOOL_GEEE };

	if (run_ioctl(fd, ifr, &eee)) {
		perror("ETHTOOL_GEEE");
		return 1;
	}
	eee.cmd = ETHTOOL_SEEE;
	eee.eee_enabled = enabled;
	if (run_ioctl(fd, ifr, &eee)) {
		perror("ETHTOOL_SEEE");
		return 1;
	}
	return 0;
}

static uint32_t speed_advertisement(unsigned int speed)
{
	switch (speed) {
	case 10:
		return ADVERTISED_10baseT_Full;
	case 100:
		return ADVERTISED_100baseT_Full;
	case 1000:
		return ADVERTISED_1000baseT_Full;
	case 2500:
		return ADVERTISED_2500baseX_Full;
	default:
		return 0;
	}
}

static int advertise_speed(int fd, struct ifreq *ifr, unsigned int speed)
{
	struct ethtool_cmd settings;
	uint32_t advertised;

	if (get_settings(fd, ifr, &settings)) {
		perror("ETHTOOL_GSET");
		return 1;
	}
	advertised = speed_advertisement(speed);
	if (!advertised) {
		fprintf(stderr, "unsupported speed: %u\n", speed);
		return 1;
	}
	settings.cmd = ETHTOOL_SSET;
	settings.autoneg = AUTONEG_ENABLE;
	settings.advertising &= ADVERTISED_Pause | ADVERTISED_Asym_Pause;
	settings.advertising |= ADVERTISED_Autoneg | ADVERTISED_TP | advertised;
	if (run_ioctl(fd, ifr, &settings)) {
		perror("ETHTOOL_SSET");
		return 1;
	}
	return 0;
}

static int advertise_all(int fd, struct ifreq *ifr)
{
	struct ethtool_cmd settings;
	uint32_t speeds = ADVERTISED_10baseT_Full |
		ADVERTISED_100baseT_Full | ADVERTISED_1000baseT_Full |
		ADVERTISED_2500baseX_Full;

	if (get_settings(fd, ifr, &settings)) {
		perror("ETHTOOL_GSET");
		return 1;
	}
	settings.cmd = ETHTOOL_SSET;
	settings.autoneg = AUTONEG_ENABLE;
	settings.advertising &= ADVERTISED_Pause | ADVERTISED_Asym_Pause;
	settings.advertising |= ADVERTISED_Autoneg | ADVERTISED_TP |
		(settings.supported & speeds);
	if (run_ioctl(fd, ifr, &settings)) {
		perror("ETHTOOL_SSET");
		return 1;
	}
	return 0;
}

int main(int argc, char **argv)
{
	struct ifreq ifr = {};
	int fd;
	int ret;

	if (argc < 3) {
		fprintf(stderr, "usage: %s IFACE show|stats|regs|eee on|off|advertise SPEED|all\n",
			argv[0]);
		return 2;
	}
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		perror("socket");
		return 1;
	}
	strncpy(ifr.ifr_name, argv[1], IFNAMSIZ - 1);
	if (!strcmp(argv[2], "show"))
		ret = show(fd, &ifr);
	else if (!strcmp(argv[2], "stats"))
		ret = show_stats(fd, &ifr);
	else if (!strcmp(argv[2], "regs"))
		ret = show_regs(fd, &ifr);
	else if (argc == 4 && !strcmp(argv[2], "eee"))
		ret = set_eee(fd, &ifr, !strcmp(argv[3], "on"));
	else if (argc == 4 && !strcmp(argv[2], "advertise"))
		ret = !strcmp(argv[3], "all") ? advertise_all(fd, &ifr) :
			advertise_speed(fd, &ifr, strtoul(argv[3], NULL, 0));
	else {
		fprintf(stderr, "invalid command\n");
		ret = 2;
	}
	close(fd);
	return ret;
}
