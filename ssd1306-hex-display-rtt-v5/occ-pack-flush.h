#ifndef __OCC_PACK_FLUSH_H__
#define __OCC_PACK_FLUSH_H__

/* Optimistic Concurrency Control (OCC) */
/* helps safely modify shared hardware registers */

/* assumes all users go though OCC for certain shared hardware registers    */
/* each (potentially concurrent) user get their own set of unpacked data    */
/* unpacked data contains shared state and acts like a write through cache  */
/* after modifying any unpacked data the user must execute pack and flush   */
/* remember to separate unpacked and packed data, packed should be on stack */

/* as an example, if there are N bitfields in a single hardware register to */
/* control separately, then the following design could be used:             */
/* - create a structure containing occ_pack_flush_head and unpacked data    */
/*   the unpacked data could in this case for instance be say N x uint8_t   */
/*   callbacks may use offset_of() to go from occ_pack_flush_head           */
/* - create a packed data structure, it should mimic the hardware register  */
/* - the init() callback initializes unpacked data from the hardware regs   */
/* - the pack() callback takes unpacked data and writes it into packed      */
/* - the flush() callback takes passed packed data and writes to hardware   */
/* - the has_changed() callback compares two versions of packed data        */

/* assumes there is a single user for each unpacked data structure member   */
/* after flushing, the code compares the packed data, if changed this       */
/* means concurrent updates have taken place and we need to repeat          */
/* once packed data has been verified to be unchanged we no longer need to  */
/* repeat the pack and flush sequence and we can consider ourselves done    */

struct occ_pack_flush_head;

struct occ_pack_flush_ops {
  void (*init)(struct occ_pack_flush_head *);
  void (*pack)(struct occ_pack_flush_head *, void *pack);
  void (*flush)(struct occ_pack_flush_head *, void *pack);
  int (*has_changed)(struct occ_pack_flush_head *,
                     void *pack_a, void *pack_b);
};

typedef struct occ_pack_flush_head {
  struct occ_pack_flush_ops *ops;
} occ_pack_flush_head_t;

static inline void
occ_pack_flush_init(occ_pack_flush_head_t *c, struct occ_pack_flush_ops *ops)
{
  c->ops = ops;
  ops->init(c);
}

static inline void
occ_pack_flush(occ_pack_flush_head_t *c, void *pack_a, void *pack_b)
{
  struct occ_pack_flush_ops *ops = c->ops;

  do {
    ops->pack(c, pack_a);
    ops->flush(c, pack_a);

    ops->pack(c, pack_b);
    ops->flush(c, pack_b);
  } while (ops->has_changed(c, pack_a, pack_b));
}

#endif /*__OCC_PACK_FLUSH_H__ */
