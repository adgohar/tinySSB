// repo.cpp

// tinySSB for ESP32
// (c) 2022-2023 <christian.tschudin@unibas.ch>

#include "tinySSBlib.h"

// ----------------------------------------------------------------------

void Repo2Class::clean(char *path)
{
  File fdir = MyFS.open(path);
  if (fdir) {
    File f = fdir.openNextFile(FILE_READ);
    while (f) {
      char *fn = strdup(f.path());
      if (f.isDirectory()) {
        this->clean(fn);
        f.close();
        MyFS.rmdir(fn);
      } else {
        f.close();
        MyFS.remove(fn);
      }
      free(fn);
      f = fdir.openNextFile(FILE_READ);
    }
    fdir.close();
  }
}

void Repo2Class::reset(char *path)
{
  if (path == NULL) { path = FEED_DIR; }
  Serial.printf("repo reset of path %s\r\n", path);
  this->clean(path);
  if (path != NULL)
    MyFS.rmdir(path);

  esp_restart(); // FIXME?? is this still necessary? if not, then we have to erase the in-memory GOset ...
}

void Repo2Class::load()
{
  File fdir = MyFS.open(FEED_DIR);
  if (!fdir) {
    Serial.printf("can't open %s\r\n", FEED_DIR);
    return;
  }
  File f = fdir.openNextFile(FILE_READ);
  while (f) {
    char* fn = (char*) f.name();
    // Serial.printf("repo load %s\r\n", fn);
    unsigned char *fid = from_hex(fn, FID_LEN); // from_b64(pos, FID_LEN)
    if (fid != NULL)
      add_replica(fid);
    f.close();
    f = fdir.openNextFile(FILE_READ);
    theUI->loop();
  }
  fdir.close();

  // init the GOset with the found keys
  for (int i = 0; i < rplca_cnt; i++)
    theGOset->populate(replicas[i]->fid);
  theGOset->populate(NULL); // triggers sorting, and setting the want_dmx
}


void Repo2Class::loop()
{
  static int done_chunk_counting = false;

  if (!done_chunk_counting) {
    int i;
    for (i = 0; i < rplca_cnt; i++)
      if (replicas[i]->chunk_cnt < 0) {
        int cnt = replicas[i]->load_chunk_cnt();
        chunk_cnt += cnt;
        // should re-sum all feed chunk counts
        theUI->refresh();
        break;
      }
    if (i >= rplca_cnt)
      done_chunk_counting = true;
  }
}


void Repo2Class::add_replica(unsigned char *fid)
{
  ReplicaClass *r = new ReplicaClass(FEED_DIR, fid);
  replicas[rplca_cnt++] = r;
  // int ndx = theGOset->_key_index(fid);

  // arm DMXT
  unsigned char dmx_val[DMX_LEN];
  int ns = r->get_next_seq(dmx_val);
  theDmx->arm_dmx(dmx_val, incoming_entry, r->fid, /*ndx,*/ ns);
  // Serial.printf("   armed %s for %d.%d\r\n", to_hex(dmx_val, 7),
  //               ndx, f->next_seq);

  /*
  // arm CHKT
  struct bipf_s *p = r->get_open_chains();
  if (p != NULL && p->cnt > 0) {
    for (int j = 0; j < p->cnt; j++) {
      int seq = p->u.dict[2*j]->u.i;
      int cnr = p->u.dict[2*j+1]->u.list[0]->u.i;
      unsigned char *hptr = p->u.dict[2*j+1]->u.list[2]->u.buf;
      theDmx->arm_hsh(hptr, incoming_chunk, r->fid, seq, cnr,
                   cnr + p->u.dict[2*j+1]->u.list[1]->u.i);
    }
  }
  */

  entry_cnt += ns - 1;
  // chunk_cnt += r->get_chunk_cnt(); //delayed until chnk_vect time

  theUI->refresh();

  if (theGOset->goset_len > 0) {
    want_offs = esp_random() % theGOset->goset_len;
    chnk_offs = esp_random() % theGOset->goset_len;
  }
  
  want_is_valid = false;
  chnk_is_valid = false;
}

ReplicaClass* Repo2Class::fid2replica(unsigned char* fid) {
  int i;
  for (i = 0; i < rplca_cnt; i++)
    if (!memcmp(fid, replicas[i]->fid, FID_LEN))
      break;
  if (i >= rplca_cnt)
    return NULL;
  return replicas[i];
}


void Repo2Class::mk_want_vect()
{
  // the following prevents rotation:
  // if (want_is_valid)
  //  return;

  String v = "";
  struct bipf_s *lptr = bipf_mkList();
  int encoding_len = bipf_encodingLength(lptr);

  bipf_list_append(lptr, bipf_mkInt(want_offs));

  int new_want_offs = want_offs + 1;
  for (int i = 0; i < theGOset->goset_len; i++) {
    unsigned int ndx = (want_offs + i) % theGOset->goset_len;
    unsigned char *fid = theGOset->get_key(ndx);
    ReplicaClass *r = theRepo->fid2replica(fid);

    new_want_offs++;
    int ns = r->get_next_seq(NULL);
    struct bipf_s *bptr = bipf_mkInt(ns);
    encoding_len += bipf_encodingLength(bptr);
    bipf_list_append(lptr, bptr);

    v += (v.length() == 0 ? "[ " : " ") + String(ndx) + "." + String(ns);
    if (encoding_len > 100)
      break;
    io_loop();
    // io_dequeue();
  }
  want_offs = new_want_offs % theGOset->goset_len;

  free(want_vect);
  if (lptr->cnt > 1) {
    want_len = bipf_encodingLength(lptr);
    want_vect = (unsigned char*) malloc(want_len);
    bipf_encode(want_vect, lptr);
    Serial.printf("   our DreQ=%s ] %dB\r\n", v.c_str(), want_len);
  } else
    want_len = 0;

  bipf_free(lptr);
  // want_is_valid = 1;
}


void Repo2Class::mk_chnk_vect()
{
  /*
#ifdef TINYSSB_BOARD_TDECK
    chnk_len = 0;
    return;
#endif
  */

  if (theGOset->goset_len == 0)
    chnk_len = 0;
  else {
    // randomized start point, instead of round-robin
    int ndx = esp_random() % theGOset->goset_len;
    String v = "";
    struct bipf_s *lptr = bipf_mkList();
    int encoding_len = bipf_encodingLength(lptr);

    int old_ndx = ndx;
    do {
      struct chunk_needed_s table[4];
      ndx = (ndx+1) % theGOset->goset_len;
      ReplicaClass *r = fid2replica(theGOset->get_key(ndx));
      io_loop();
      // Serial.println("asking for open sidechains");
      int n = r->get_open_sidechains(4, table);
      // Serial.printf(" --> n=%d\r\n", n);
      for (int i = 0; i < n; i++) {
        // Serial.printf("  h=%s s=%d c=%d\r\n",
        //               to_hex(table[i].hash, HASH_LEN),
        //               table[i].snr, table[i].cnr);
        struct bipf_s *c = bipf_mkList();
        bipf_list_append(c, bipf_mkInt(ndx));
        bipf_list_append(c, bipf_mkInt(table[i].snr));
        bipf_list_append(c, bipf_mkInt(table[i].cnr));
        encoding_len += bipf_encodingLength(c);
        bipf_list_append(lptr, c);
        theDmx->arm_hsh(table[i].hash, incoming_chunk,
                        theGOset->get_key(ndx), table[i].snr, table[i].cnr, 0);
        v += (v.length() == 0 ? "[ " : " ") + String(ndx) + "."
                           + String(table[i].snr) + '.' + String(table[i].cnr);
        if (encoding_len > 100)
          break;
      }
    } while (encoding_len <= 100 && old_ndx != ndx);
    free(chnk_vect);
    chnk_vect = NULL;
    if (lptr->cnt > 0) {
      chnk_len = bipf_encodingLength(lptr);
      chnk_vect = (unsigned char*) malloc(chnk_len);
      bipf_encode(chnk_vect, lptr);
      Serial.printf("   our CreQ=%s ] %dB\r\n", v.c_str(), chnk_len);
    } else
      chnk_len = 0;
    bipf_free(lptr);
  }
}


// eof
