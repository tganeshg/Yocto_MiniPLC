#include "project_store.h"

#include "../core/paths.h"
#include "../ladder/ladder_vm.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static int mkdir_p(const char *path)
{
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = 0;

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
                return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
        return -1;
    return 0;
}

static int run_sh(const char *cmd)
{
    int r = system(cmd);
    if (r != 0)
        return -1;
    return 0;
}

static ssize_t read_file_alloc(const char *path, uint8_t **out_buf)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return -1;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    long sz = ftell(f);
    if (sz < 0 || sz > 8 * 1024 * 1024) {
        fclose(f);
        return -1;
    }
    rewind(f);
    uint8_t *b = (uint8_t *)malloc((size_t)sz);
    if (!b) {
        fclose(f);
        return -1;
    }
    if (fread(b, 1, (size_t)sz, f) != (size_t)sz) {
        free(b);
        fclose(f);
        return -1;
    }
    fclose(f);
    *out_buf = b;
    return (ssize_t)sz;
}

static int install_file(const char *src, const char *dst)
{
    FILE *in = fopen(src, "rb");
    if (!in)
        return -1;
    FILE *out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return -1;
    }
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            return -1;
        }
    }
    fclose(in);
    fclose(out);
    return 0;
}

int project_store_load_persisted_program(void)
{
    uint8_t *buf = NULL;
    ssize_t n = read_file_alloc(MINIPLC_PROGRAM_BIN, &buf);
    if (n < 0 || !buf) {
        ladder_vm_clear_program();
        return -ENOENT;
    }
    int r = ladder_vm_load_blob(buf, (size_t)n);
    free(buf);
    return r;
}

int project_store_apply_zip(void)
{
    struct stat st;
    if (stat(MINIPLC_UPLOAD_ZIP, &st) != 0)
        return -ENOENT;

    char cmd[768];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s' && mkdir -p '%s'", MINIPLC_STAGING_DIR, MINIPLC_STAGING_DIR);
    if (run_sh(cmd) != 0)
        return -EIO;

    snprintf(cmd, sizeof(cmd), "/usr/bin/unzip -oq '%s' -d '%s'", MINIPLC_UPLOAD_ZIP, MINIPLC_STAGING_DIR);
    if (run_sh(cmd) != 0)
        return -EIO;

    char ladder_path[512];
    snprintf(ladder_path, sizeof(ladder_path), "%s/ladder.plcbin", MINIPLC_STAGING_DIR);

    uint8_t *buf = NULL;
    ssize_t n = read_file_alloc(ladder_path, &buf);
    if (n < 0 || !buf)
        return -EINVAL;

    int lr = ladder_vm_load_blob(buf, (size_t)n);
    free(buf);
    if (lr != 0)
        return -EINVAL;

    mkdir_p("/etc/plc");
    if (install_file(ladder_path, MINIPLC_PROGRAM_BIN) != 0)
        return -EIO;

    mkdir_p(MINIPLC_HMI_DIR);
    snprintf(cmd, sizeof(cmd), "rm -rf '%s/'* 2>/dev/null; cp -r '%s/hmi/.' '%s/' 2>/dev/null || true",
             MINIPLC_HMI_DIR, MINIPLC_STAGING_DIR, MINIPLC_HMI_DIR);
    (void)run_sh(cmd);

    sync();
    return 0;
}
