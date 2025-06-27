#include <stdio.h>
#include <sai.h>

#define CHECK(status, msg)                                    \
    do {                                                      \
        if ((status) != SAI_STATUS_SUCCESS) {                 \
            fprintf(stderr, "%s failed: %d\n", msg, status);  \
            goto cleanup;                                     \
        }                                                     \
    } while (0)

static const sai_object_id_t SWITCH_ID = 0x21000000000000ULL;

int main(void)
{
    /* ハンドル */
    sai_object_id_t match_id = 0, group_id = 0, udf_id = 0, hash_id = 0;

    /* API テーブルを 1 個だけ取得 */
    sai_udf_api_t   *udf_api   = NULL;
    sai_hash_api_t  *hash_api  = NULL;
    sai_switch_api_t *sw_api   = NULL;

    CHECK(sai_api_initialize(0, NULL), "sai_api_initialize");
    CHECK(sai_api_query(SAI_API_UDF,   (void **)&udf_api),  "query UDF");
    CHECK(sai_api_query(SAI_API_HASH,  (void **)&hash_api), "query HASH");
    CHECK(sai_api_query(SAI_API_SWITCH,(void **)&sw_api),   "query SWITCH");

    /* 1. IPv4 を対象にする UDF_MATCH */
    sai_attribute_t m;
    m.id = SAI_UDF_ATTR_MATCH_L2_TYPE;
    m.value.u16 = 0x0800;                             /* Ethertype = IPv4 */
    CHECK(udf_api->create_udf_match(&match_id,
                                    SWITCH_ID,
                                    1, &m),
          "create_udf_match");

    /* 2. ハッシュ用 UDF_GROUP */
    sai_attribute_t g;
    g.id = SAI_UDF_GROUP_ATTR_TYPE;
    g.value.s32 = SAI_UDF_GROUP_TYPE_HASH;
    CHECK(udf_api->create_udf_group(&group_id,
                                    SWITCH_ID,
                                    1, &g),
          "create_udf_group");

    /* 3. TTL 抽出 UDF (L3 offset 8, length 1) */
    sai_attribute_t a[5];
    a[0].id = SAI_UDF_ATTR_MATCH_ID;   a[0].value.oid = match_id;
    a[1].id = SAI_UDF_ATTR_GROUP_ID;   a[1].value.oid = group_id;
    a[2].id = SAI_UDF_ATTR_BASE;       a[2].value.s32 = SAI_UDF_BASE_L3;
    a[3].id = SAI_UDF_ATTR_OFFSET;     a[3].value.u16 = 8;
    a[4].id = SAI_UDF_ATTR_LENGTH;     a[4].value.u16 = 1;
    CHECK(udf_api->create_udf(&udf_id,
                              SWITCH_ID,
                              5, a),
          "create_udf");

    /* 4. HASH オブジェクトを作成し UDF_GROUP を 1 つ登録 */
    sai_attribute_t h;
    h.id = SAI_HASH_ATTR_UDF_GROUP_LIST;
    h.value.objlist.count = 1;
    h.value.objlist.list  = &group_id;
    CHECK(hash_api->create_hash(&hash_id,
                                SWITCH_ID,
                                1, &h),
          "create_hash");

    /* 5. スイッチ属性を更新して ECMP ハッシュに適用 */
    sai_attribute_t sw;
    sw.id = SAI_SWITCH_ATTR_ECMP_HASH;      /* SAI 1.7+ 統合属性 */
    sw.value.oid = hash_id;
    CHECK(sw_api->set_switch_attribute(SWITCH_ID, &sw),
          "set_switch_attribute");

    printf("TTL-based ECMP hash setup completed successfully.\n");
    return 0;

cleanup:                                   /* エラー発生時の後片付け */
    if (hash_id)  hash_api->remove_hash(hash_id);
    if (udf_id)   udf_api->remove_udf(udf_id);
    if (group_id) udf_api->remove_udf_group(group_id);
    if (match_id) udf_api->remove_udf_match(match_id);
    return 1;
}
