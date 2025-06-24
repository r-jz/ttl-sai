/*
 * ttl_udf_hash.c   –  SAI 1.10+ / libsaivs.so.0
 * 既存スイッチ ID = 0x21000000000000
 * IPv4 TTL (1 byte) を UDF で抜き、ECMP ハッシュキーへ追加。
 *
 * gcc -Wall -Werror -O2 -I/home/inc ttl_udf_hash.c \
 *     /lib/x86_64-linux-gnu/libsaivs.so.0 -lpthread -lrt -o ttl_udf_hash
 */

#include <stdio.h>
#include <sai.h>

/*== 1. 必須：スイッチプロファイル用ダミーコールバック ============*/
static const char *dummy_profile_get_value(_In_ sai_profile_get_value_type_t key)
{
    (void)key;
    return NULL;                 /* 返す値が無い場合は NULL で OK */
}

static int dummy_profile_get_next_value(
        _Inout_ sai_profile_get_next_value_type_t *key,
        _Inout_ sai_profile_get_next_value_type_t *value)
{
    (void)key; (void)value;
    return -1;                   /* -1 で列挙終了を示す */
}

static const sai_service_method_table_t services = {
    .profile_get_value      = dummy_profile_get_value,
    .profile_get_next_value = dummy_profile_get_next_value
};
/*===============================================================*/

/* ANSI colors (任意) */
#define C_RED "\033[31m"
#define C_GRN "\033[32m"
#define C_RST "\033[0m"

static const sai_object_id_t SWITCH_ID = 0x21000000000000ULL;

/* 最小限の sai_status 文字列化ヘルパ */
static const char *sts(sai_status_t s){
    switch(s){
        case SAI_STATUS_SUCCESS:           return "SUCCESS";
        case SAI_STATUS_INVALID_PARAMETER: return "INVALID_PARAMETER";
        case SAI_STATUS_NOT_SUPPORTED:     return "NOT_SUPPORTED";
        default:;
    }
    static char buf[16]; snprintf(buf,sizeof(buf),"0x%x",s); return buf;
}

#define TRY(call, label)                                                       \
    do{ sai_status_t rc=(call);                                                \
        if(rc!=SAI_STATUS_SUCCESS){                                            \
            fprintf(stderr,C_RED"[ERR]"C_RST" %s → %s\n", label, sts(rc));     \
            goto cleanup;                                                      \
        } else {                                                               \
            fprintf(stderr,C_GRN"[OK]"C_RST"  %s\n", label);                   \
        } }while(0)

int main(void)
{
    sai_udf_api_t    *udf  = NULL;
    sai_hash_api_t   *hash = NULL;
    sai_switch_api_t *sw   = NULL;

    sai_object_id_t match=0, grp=0, udfid=0, hsh=0;

    /* 0. SAI 初期化（NULL → &services に修正） */
    TRY( sai_api_initialize(0, &services),   "sai_api_initialize" );
    TRY( sai_api_query(SAI_API_UDF,    (void**)&udf),  "query UDF API" );
    TRY( sai_api_query(SAI_API_HASH,   (void**)&hash), "query HASH API" );
    TRY( sai_api_query(SAI_API_SWITCH, (void**)&sw),   "query SWITCH API" );

    /* 1. UDF_MATCH : L2 Ethertype = IPv4 */
    sai_attribute_t mat;
    mat.id = SAI_UDF_MATCH_ATTR_L2_TYPE;
    mat.value.u16 = 0x0800;
    TRY( udf->create_udf_match(&match, SWITCH_ID, 1, &mat),
         "create_udf_match" );

    /* 2. UDF_GROUP : HASH 用 + LENGTH=1 */
    sai_attribute_t g[2];
    g[0].id = SAI_UDF_GROUP_ATTR_TYPE;   g[0].value.s32 = SAI_UDF_GROUP_TYPE_HASH;
    g[1].id = SAI_UDF_GROUP_ATTR_LENGTH; g[1].value.u16 = 1;
    TRY( udf->create_udf_group(&grp, SWITCH_ID, 2, g),
         "create_udf_group" );

    /* 3. UDF : TTL (L3 offset 8) */
    sai_attribute_t a[4] = {
        { SAI_UDF_ATTR_MATCH_ID, .value.oid = match },
        { SAI_UDF_ATTR_GROUP_ID, .value.oid = grp   },
        { SAI_UDF_ATTR_BASE,     .value.s32 = SAI_UDF_BASE_L3 },
        { SAI_UDF_ATTR_OFFSET,   .value.u16 = 8 }
    };
    TRY( udf->create_udf(&udfid, SWITCH_ID, 4, a),
         "create_udf TTL" );

    /* 4. HASH オブジェクト */
    sai_attribute_t h;
    h.id = SAI_HASH_ATTR_UDF_GROUP_LIST;
    h.value.objlist.count = 1;
    h.value.objlist.list  = &grp;
    TRY( hash->create_hash(&hsh, SWITCH_ID, 1, &h),
         "create_hash" );

    /* 5. ECMP ハッシュへ適用 */
    sai_attribute_t swattr;
    swattr.id = SAI_SWITCH_ATTR_ECMP_HASH;
    swattr.value.oid = hsh;
    TRY( sw->set_switch_attribute(SWITCH_ID, &swattr),
         "set_switch_attribute" );

    printf(C_GRN"\nTTL-based ECMP hash configured successfully.\n"C_RST);
    return 0;

cleanup:
    if(hsh)   hash->remove_hash(hsh);
    if(udfid) udf->remove_udf(udfid);
    if(grp)   udf->remove_udf_group(grp);
    if(match) udf->remove_udf_match(match);
    return 1;
}

