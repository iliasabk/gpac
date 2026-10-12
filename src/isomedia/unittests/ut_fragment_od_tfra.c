#include "tests.h"
#include <gpac/internal/isomedia_dev.h>
#include <gpac/mpeg4_odf.h>

unittest(fragment_od_sample_tfra_uaf)
{
	/* Regression for gpac/gpac#3964: gf_isom_fragment_add_sample_ex repointed
	 * `sample` to the OD-parsed `od_sample`, freed it, then the tfra block
	 * still read sample->IsRAP/DTS/CTS_Offset (heap-use-after-free). */
#if !defined(GPAC_DISABLE_ISOM) && !defined(GPAC_DISABLE_ISOM_WRITE) && !defined(GPAC_DISABLE_ISOM_FRAGMENTS)
	GF_Err e;
	GF_ISOFile *movie = NULL;
	GF_ODCodec *enc = NULL;
	GF_ODUpdate *upd = NULL;
	GF_ObjectDescriptor *od = NULL;
	GF_ISOSample s;
	GF_TrackExtendsBox *trex;
	u8 *au = NULL;
	u32 au_len = 0;
	u32 i, trackNum;
	char path[GF_MAX_PATH];
	const char *tmp = getenv("TMPDIR");

	snprintf(path, sizeof(path), "%s/gpac_ut_frag_od_tfra.mp4", tmp ? tmp : ".");
	movie = gf_isom_open(path, GF_ISOM_OPEN_WRITE, NULL);
	assert_not_null(movie);
	if (!movie) return;

	e = gf_isom_enable_mfra(movie);
	assert_equal(e, GF_OK, "%d");

	trackNum = gf_isom_new_track(movie, 1, GF_ISOM_MEDIA_OD, 1000);
	assert_equal(trackNum, 1, "%u");
	e = gf_isom_setup_track_fragment(movie, 1, 1, 1000, 0, 0, 0, 0, GF_FALSE);
	assert_equal(e, GF_OK, "%d");
	e = gf_isom_finalize_for_fragment(movie, 0, GF_FALSE);
	assert_equal(e, GF_OK, "%d");
	e = gf_isom_start_fragment(movie, GF_ISOM_FRAG_MOOF_FIRST);
	assert_equal(e, GF_OK, "%d");

	/* minimal valid OD access unit: one ODUpdate with one ObjectDescriptor */
	enc = gf_odf_codec_new();
	upd = (GF_ODUpdate *) gf_odf_com_new(GF_ODF_OD_UPDATE_TAG);
	od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
	assert_not_null(enc);
	assert_not_null(upd);
	assert_not_null(od);
	if (!enc || !upd || !od) {
		if (od) gf_odf_desc_del((GF_Descriptor *) od);
		if (upd) gf_odf_com_del((GF_ODCom **) &upd);
		if (enc) gf_odf_codec_del(enc);
		gf_isom_delete(movie);
		gf_file_delete(path);
		return;
	}
	od->objectDescriptorID = 1;
	gf_list_add(upd->objectDescriptors, od);
	e = gf_odf_codec_add_com(enc, (GF_ODCom *) upd);
	assert_equal(e, GF_OK, "%d");
	e = gf_odf_codec_encode(enc, 0);
	assert_equal(e, GF_OK, "%d");
	e = gf_odf_codec_get_au(enc, &au, &au_len);
	assert_equal(e, GF_OK, "%d");
	assert_true(au != NULL && au_len > 0);

	memset(&s, 0, sizeof(s));
	s.data = au;
	s.dataLength = au_len;
	s.IsRAP = RAP;
	e = gf_isom_fragment_add_sample(movie, 1, &s, 1, 1000, 0, 0, GF_FALSE);
	assert_equal(e, GF_OK, "%d");

	/* the tfra entry must have been filled from the sample, not skipped */
	for (i = 0; i < gf_list_count(movie->moov->mvex->TrackExList); i++) {
		trex = (GF_TrackExtendsBox *) gf_list_get(movie->moov->mvex->TrackExList, i);
		if (trex->trackID == 1) break;
		trex = NULL;
	}
	assert_not_null(trex);
	if (trex && trex->tfra && trex->tfra->nb_entries) {
		assert_equal(trex->tfra->entries[0].trun_number, 1, "%u");
		assert_equal(trex->tfra->entries[0].sample_number, 1, "%u");
	} else {
		assert_true(trex && trex->tfra && trex->tfra->nb_entries > 0);
	}

	if (enc) gf_odf_codec_del(enc);
	gf_isom_delete(movie);
	gf_file_delete(path);
#endif
}
