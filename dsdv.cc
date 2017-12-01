/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/*
 * Copyright (c) 1997 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Ported from CMU/Monarch's code, nov'98 -Padma.
 *
 * $Header: /cvsroot/nsnam/ns-2/dsdv/dsdv.cc,v 1.26 2006/02/21 15:20:18 mahrenho Exp $
 */

extern "C" {
#include <stdarg.h>
#include <float.h>
}

#include "dsdv.h"
#include "priqueue.h"

#include <random.h>

#include <cmu-trace.h>
#include <address.h>
#include <mobilenode.h>
#include <stdio.h>
#include <stdlib.h>

#define DSDV_STARTUP_JITTER 2.0	// secs to jitter start of periodic activity from
				// when start-dsr msg sent to agent  
#define DSDV_ALMOST_NOW     0.1 // jitter used for events that should be effectively
				// instantaneous but are jittered to prevent
				// synchronization
#define DSDV_BROADCAST_JITTER 0.01 // jitter for all broadcast packets
#define DSDV_MIN_TUP_PERIOD 1.0 // minimum time between triggered updates
#define IP_DEF_TTL   32 // default TTTL

#undef TRIGGER_UPDATE_ON_FRESH_SEQNUM
//#define TRIGGER_UPDATE_ON_FRESH_SEQNUM
/* should the receipt of a fresh (newer) sequence number cause us
   to send a triggered update?  If undef'd, we'll only trigger on
   routing metric changes */



// Returns a random number between 0 and max
static inline double 
jitter (double max, int be_random_)
{
  return (be_random_ ? Random::uniform(max) : 0);
}



void DSDV_Agent::
trace (char *fmt,...)
{
  FILE *fp = fopen("dsdv.log","a+");
  fprintf(fp, "DSDV_Agent::trace\n");
  
  va_list ap;

  if (!tracetarget)
    return;

  va_start (ap, fmt);
  vsprintf (tracetarget->pt_->buffer (), fmt, ap);
  tracetarget->pt_->dump ();
  va_end (ap);

  fclose(fp);
}

void 
DSDV_Agent::tracepkt (Packet * p, double now, int me, const char *type)
{
  FILE *fp = fopen("dsdv.log","a+");
  fprintf(fp, "DSDV_Agent::tracepkt\n");
  
  char buf[1024]; // untuk menyimpan buffer paket, apa itu buffer, inget waktu jarkom

  unsigned char *walk = p->accessdata ();

  int ct = *(walk++);
  int seq, dst, met;

  snprintf (buf, 1024, "V%s %.5f _%d_ [%d]:", type, now, me, ct); // penyimpanan di buffer dg ukuran 1024. 
  fprintf(fp, "di trace paket :  type : V%s now : %.5f me: _%d_ ct: [%d]:", type, now, me, ct);
  while (ct--)
    {
      dst = *(walk++);
      dst = dst << 8 | *(walk++);
      dst = dst << 8 | *(walk++);
      dst = dst << 8 | *(walk++);
      met = *(walk++);
      seq = *(walk++);
      seq = seq << 8 | *(walk++);
      seq = seq << 8 | *(walk++);
      seq = seq << 8 | *(walk++);
      snprintf (buf, 1024, "%s (%d,%d,%d)", buf, dst, met, seq);
      (fp, "di trace paket :(dst: %d,met: %d,seq: %d)", dst, met, seq);
    }

  fprintf(fp, "\n");

  // Now do trigger handling.
  //trace("VTU %.5f %d", now, me);
  if (verbose_)
    fprintf(fp, "Masuk jika update krn triger\n");
    trace ("%s", buf);
    fprintf(fp, "%s", buf);
  fclose(fp);
}

// Prints out an rtable element.
void
DSDV_Agent::output_rte(const char *prefix, rtable_ent * prte, DSDV_Agent * a)
{
  FILE *fp = fopen("dsdv.log","a+");
  fprintf(fp, "DSDV_Agent::output_rte\n");
  fclose(fp);
  a->trace("DFU: deimplemented");
  printf("DFU: deimplemented");

  prte = 0;
  prefix = 0;
#if 0
  printf ("%s%d %d %d %d %f %f %f %f 0x%08x\n",
	  prefix, prte->dst, prte->hop, prte->metric, prte->seqnum,
	  prte->udtime, prte->new_seqnum_at, prte->wst, prte->changed_at,
	  (unsigned int) prte->timeout_event);
  a->trace ("VTE %.5f %d %d %d %d %f %f %f %f 0x%08x",
          Scheduler::instance ().clock (), prte->dst, prte->hop, prte->metric,
	  prte->seqnum, prte->udtime, prte->new_seqnum_at, prte->wst, prte->changed_at,
	    prte->timeout_event);
#endif
}

class DSDVTriggerHandler : public Handler {
  public:
    DSDVTriggerHandler(DSDV_Agent *a_) { a = a_; }
    virtual void handle(Event *e);
  private:
    DSDV_Agent *a;
};


void
DSDVTriggerHandler::handle(Event *e)
 // send a triggered update (or a full update if one's needed)
{
FILE *fp = fopen("dsdv.log","a+");
fprintf(fp, "\n\nDSDVTriggerHandler::handle\n");

	//DEBUG
	//printf("(%d)-->triggered update with e=%x\n", a->myaddr_,e);
  fprintf(fp, "(myaddr: %d)-->triggered update with e=%x\n", a->myaddr_,e); 

  Scheduler & s = Scheduler::instance ();
  Time now = s.clock ();
  rtable_ent *prte;
  int update_type;	 // we want periodic (=1) or triggered (=0) update?
  Time next_possible = a->lasttup_ + DSDV_MIN_TUP_PERIOD;

  for (a->table_->InitLoop(); (prte = a->table_->NextLoop());)
	  if (prte->trigger_event == e) break;

  assert(prte && prte->trigger_event == e);

  if (now < next_possible)
    {
	    //DEBUG
	    //printf("(%d)..Re-scheduling triggered update\n",a->myaddr_);
      fprintf(fp, "(myaddr_: %d)..Re-scheduling triggered update\n",a->myaddr_);
      s.schedule(a->trigger_handler, e, next_possible - now);
      a->cancelTriggersBefore(next_possible);
      return;
    }

  update_type = 0;
  Packet * p = a->makeUpdate(/*in-out*/update_type);
      
  if (p != NULL) 
    {	  
      if (update_type == 1)
	{ // we got a periodic update, though we only asked for triggered
	  // cancel and reschedule periodic update
	  s.cancel(a->periodic_callback_);
	  //DEBUG
	  //printf("we got a periodic update, though asked for trigg\n");
    fprintf(fp, "we got a periodic update, though asked for trigg\n");
	  s.schedule (a->helper_, a->periodic_callback_, 
		      a->perup_ * (0.75 + jitter (0.25, a->be_random_)));
	  if (a->verbose_) a->tracepkt (p, now, a->myaddr_, "PU");	  
	}
      else
	{
	  if (a->verbose_) a->tracepkt (p, now, a->myaddr_, "TU");	  
	}
      assert (!HDR_CMN (p)->xmit_failure_);	// DEBUG 0x2	  
      s.schedule (a->target_, p, jitter(DSDV_BROADCAST_JITTER, a->be_random_));
      
      a->lasttup_ = now; // even if we got a full update, it still counts
      // for our last triggered update time
    }
  
  // free this event
  for (a->table_->InitLoop (); (prte = a->table_->NextLoop ());)
    if (prte->trigger_event && prte->trigger_event == e)
      {
	prte->trigger_event = 0;
	delete e;
      }

  fclose(fp);
}

void
DSDV_Agent::cancelTriggersBefore(Time t)
  // Cancel any triggered events
  // t (exclusive)scheduled to take place *before* time
{
  FILE *fp = fopen("dsdv.log","a+");
  fprintf(fp, "\n\nDSDV_Agent::cancelTriggersBefore\n");
  fprintf(fp, "my address : %d \n", myaddr_);
  
  rtable_ent *prte;
  Scheduler & s = Scheduler::instance ();

  for (table_->InitLoop (); (prte = table_->NextLoop ());)
    if (prte->trigger_event && prte->trigger_event->time_ < t)
      {
	      //DEBUG
        fprintf(fp, "(%d) cancel event %x\n",myaddr_,prte->trigger_event);
	      //printf("(%d) cancel event %x\n",myaddr_,prte->trigger_event);
	s.cancel(prte->trigger_event);
	delete prte->trigger_event;
	prte->trigger_event = 0;
      }
  fclose(fp);
}

void
DSDV_Agent::needTriggeredUpdate(rtable_ent *prte, Time t)
  // if no triggered update already pending, make one so
{
  FILE *fp = fopen("dsdv.log","a+");
  fprintf(fp, "\n\nDSDV_Agent::needTriggeredUpdate\n");
  
  Scheduler & s = Scheduler::instance();
  Time now = Scheduler::instance().clock();

  assert(t >= now);

  if (prte->trigger_event) 
    s.cancel(prte->trigger_event);
  else
    prte->trigger_event = new Event;
  //DEBUG
  //printf("(%d)..scheduling trigger-update with event %x\n",myaddr_,prte->trigger_event);
  fprintf(fp, "my address : (%d)..scheduling trigger-update with (prte->trigger_event) event %x\n",myaddr_,prte->trigger_event);
  s.schedule(trigger_handler, prte->trigger_event, t - now);
  fclose(fp);
}

void
DSDV_Agent::helper_callback (Event * e)
{
  FILE *fp = fopen("dsdv.log","a+");
  fprintf(fp, "\n \n DSDV_Agent::helper_callback\n");
  
  Scheduler & s = Scheduler::instance ();
  double now = s.clock ();
  rtable_ent *prte;
  rtable_ent *pr2;
  int update_type;	 // we want periodic (=1) or triggered (=0) update? // disini tipe update 1 kalo periodic , 0 kalo trigered
  //DEBUG
  //printf("Triggered handler on 0x%08x\n", e);
  fprintf(fp, "Triggered handler on 0x%08x\n", e);

  // Check for periodic callback. Kayaknya disini yang cek scra periodik untuk update
  if (periodic_callback_ && e == periodic_callback_)// jika update secara periodik, eventny periodic
    {
      update_type = 1; // tipe update periodik
      Packet *p = makeUpdate(/*in-out*/update_type);

      if (verbose_)
    	{
        fprintf(fp, "Jika update krn trigger \n");
    	  trace ("VPC %.5f _%d_", now, myaddr_);
    	  tracepkt (p, now, myaddr_, "PU");

        fprintf(fp, "VPC %.5f _%d_", now, myaddr_);
    	}

      
      if (p) {
        fprintf(fp, "Jika update krn periodik \n");
	      assert (!HDR_CMN (p)->xmit_failure_);	// DEBUG 0x2
	      // send out update packet jitter to avoid sync
	      //DEBUG
	      //printf("(%d)..sendout update pkt (periodic=%d)\n",myaddr_,update_type);
        fprintf(fp, "kayaknya ini broadcast : (myaddr_: %d)..sendout update pkt (periodic=%d)\n",myaddr_,update_type);
	      s.schedule (target_, p, jitter(DSDV_BROADCAST_JITTER, be_random_));
      }
      
      // put the periodic update sending callback back onto the 
      // the scheduler queue for next time....
      s.schedule (helper_, periodic_callback_, 
		  perup_ * (0.75 + jitter (0.25, be_random_)));

      // this will take the place of any planned triggered updates
      lasttup_ = now;
      return;
    }

  // Check for timeout
  // If it was a timeout, fix the routing table.
  for (table_->InitLoop (); (prte = table_->NextLoop ());)
    if (prte->timeout_event && (prte->timeout_event == e))
      break;

  // If it was a timeout, prte will be non-NULL
  // Note that in the if we don't touch the changed_at time, so that when
  // wst is computed, it doesn't consider the infinte metric the best
  // one at that sequence number.
  // Disini memperbaiki tabel routing : ketika terjadi loop maka btutuh trigerred update
  if (prte)
  {
    if (verbose_)
  	{
  	  trace ("VTO %.5f _%d_ %d->%d", now, myaddr_, myaddr_, prte->dst);
      fprintf(fp, "VTO now: %.5f myaddr_: _%d_ source: %d->destination: %d \n", now, myaddr_, myaddr_, prte->dst);
  	  /*	  trace ("VTO %.5f _%d_ trg_sch %x on sched %x time %f", now, myaddr_, 
  		 trigupd_scheduled, 
  		 trigupd_scheduled ? s.lookup(trigupd_scheduled->uid_) : 0,
  		 trigupd_scheduled ? trigupd_scheduled->time_ : 0); */
  	}
    
    for (table_->InitLoop (); (pr2 = table_->NextLoop ()); )
  	{
  	  if (pr2->hop == prte->dst && pr2->metric != BIG)
	    {
	      if (verbose_)
		    trace ("VTO %.5f _%d_ marking %d", now, myaddr_, pr2->dst);
        fprintf(fp, "VTO %.5f _%d_ marking %d \n", now, myaddr_, pr2->dst);
	      pr2->metric = BIG;
	      pr2->advertise_ok_at = now;
	      pr2->advert_metric = true;
	      pr2->advert_seqnum = true;
	      pr2->seqnum++;
	      // And we have routing info to propogate.
	      //DEBUG
	      //printf("(%d)..we have routing info to propagate..trigger update for dst %d\n",myaddr_,pr2->dst);
        fprintf(fp, "(%d)..we have routing info to propagate..trigger update for dst %d\n",myaddr_,pr2->dst);
	      needTriggeredUpdate(pr2, now);
	    }
  	}
    // OK the timeout expired, so we'll free it. No dangling pointers.
    prte->timeout_event = 0;    
  }

  else 
    { // unknown event on  queue
      fprintf(stderr,"DFU: unknown queue event\n");
     abort();
    }

  if (e)
    delete e;

  fclose(fp);
}

void
DSDV_Agent::lost_link (Packet *p)
{
  FILE *fp = fopen("dsdv.log","a+");
  fprintf(fp, "DSDV_Agent::lost_link\n");
  
  hdr_cmn *hdrc = HDR_CMN (p);
  rtable_ent *prte = table_->GetEntry (hdrc->next_hop_);

  if(use_mac_ == 0) {
          drop(p, DROP_RTR_MAC_CALLBACK);
          return;
  }

  //DEBUG
  //printf("(%d)..Lost link..\n",myaddr_);
  fprintf(fp, "(%d)..Lost link..\n",myaddr_);

  if (verbose_ && hdrc->addr_type_ == NS_AF_INET)
    trace("VLL %.8f %d->%d lost at %d",
    Scheduler::instance().clock(),
	   hdr_ip::access(p)->saddr(), hdr_ip::access(p)->daddr(),
	   myaddr_);

  if (!use_mac_ || !prte || hdrc->addr_type_ != NS_AF_INET)
    return;

  if (verbose_)
    trace ("VLP %.5f %d:%d->%d:%d lost at %d [hop %d]",
  Scheduler::instance ().clock (),
	   hdr_ip::access (p)->saddr(),
	   hdr_ip::access (p)->sport(),
	   hdr_ip::access (p)->daddr(),
	   hdr_ip::access (p)->dport(),
	   myaddr_, prte->dst);

  if (prte->timeout_event)
    {
      Scheduler::instance ().cancel (prte->timeout_event);
      helper_callback (prte->timeout_event);
    }
  else if (prte->metric != BIG)
    {
      assert(prte->timeout_event == 0);
      prte->timeout_event = new Event ();
      helper_callback (prte->timeout_event);
    }

  // Queue these packets up...
  recv(p, 0);
  fclose(fp);


#if 0
  while (p2 = ((PriQueue *) target_)->filter (prte->dst))
    {
      if (verbose_)
      trace ("VRS %.5f %d:%d->%d:%d lost at %d", Scheduler::instance ().clock (),
	       hdr_ip::access (p2)->saddr(),
	       hdr_ip::access (p2)->sport(),
	       hdr_ip::access (p2)->daddr(),
	       hdr_ip::access (p2)->dport(), myaddr_);

      recv(p2, 0);
    }

  while (p2 = ll_queue->filter (prte->dst))
    {
      if (verbose_)
      trace ("VRS %.5f %d:%d->%d:%d lost at %d", Scheduler::instance ().clock (),
	       hdr_ip::access (p2)->saddr(),
	       hdr_ip::access (p2)->sport(),
	       hdr_ip::access (p2)->daddr(),
	       hdr_ip::access (p2)->dport(), myaddr_);

      recv (p2, 0);
    }
#endif
}

static void 
mac_callback (Packet * p, void *arg)
{
  ((DSDV_Agent *) arg)->lost_link (p);
}

Packet *
DSDV_Agent::makeUpdate(int& periodic)
    // return a packet advertising the state in the routing table
    // makes a full ``periodic'' update if requested, or a ``triggered''
    // partial update if there are only a few changes and full update otherwise
    // returns with periodic = 1 if full update returned, or = 0 if partial
  //   update returned
	
{
	//DEBUG
	//printf("(%d)-->Making update pkt\n",myaddr_);
	
  Packet *p = allocpkt ();
  hdr_ip *iph = hdr_ip::access(p);
  hdr_cmn *hdrc = HDR_CMN (p);
  double now = Scheduler::instance ().clock ();
  rtable_ent *prte;
  unsigned char *walk;

  int change_count;             // count of entries to go in this update
  int rtbl_sz;			// counts total entries in rt table
  int unadvertiseable;		// number of routes we can't advertise yet

  //printf("Update packet from %d [per=%d]\n", myaddr_, periodic);

  // The packet we send wants to be broadcast
  hdrc->next_hop_ = IP_BROADCAST;
  hdrc->addr_type_ = NS_AF_INET;
  iph->daddr() = IP_BROADCAST << Address::instance().nodeshift();
  iph->dport() = ROUTER_PORT;

  change_count = 0;
  rtbl_sz = 0;
  unadvertiseable = 0;
  for (table_->InitLoop (); 
       (prte = table_->NextLoop ()); )
    {
      rtbl_sz++;
      if ((prte->advert_seqnum || prte->advert_metric) 
	  && prte->advertise_ok_at <= now) 
	change_count++;

      if (prte->advertise_ok_at > now) unadvertiseable++;
    }
  //printf("change_count = %d\n",change_count);
  if (change_count * 3 > rtbl_sz && change_count > 3)
    { // much of the table has changed, just do a periodic update now
      periodic = 1;
    }

  // Periodic update... increment the sequence number...
  if (periodic)
    {
      change_count = rtbl_sz - unadvertiseable;
      //printf("rtbsize-%d, unadvert-%d\n",rtbl_sz,unadvertiseable);
      rtable_ent rte;
      bzero(&rte, sizeof(rte));

      /* inc sequence number on any periodic update, even if we started
	 off wanting to do a triggered update, b/c we're doing a real
	 live periodic update now that'll take the place of our next
	 periodic update */
      seqno_ += 2;

      rte.dst = myaddr_;
      //rte.hop = iph->src();
      rte.hop = Address::instance().get_nodeaddr(iph->saddr());
      rte.metric = 0;
      rte.seqnum = seqno_;

      rte.advertise_ok_at = 0.0; // can always advert ourselves
      rte.advert_seqnum = true;  // always include ourselves in Triggered Upds
      rte.changed_at = now;
      rte.new_seqnum_at = now;
      rte.wst = 0;
      rte.timeout_event = 0;		// Don't time out our localhost :)

      rte.q = 0;		// Don't buffer pkts for self!

      table_->AddEntry (rte);
    }

  if (change_count == 0) 
    {
      Packet::free(p); // allocated by us, no drop needed

      return NULL; // nothing to advertise
    }

  /* ****** make the update packet.... ***********
     with less than 100+ nodes, an update for all nodes is less than the
     MTU, so don't bother worrying about splitting the update over
     multiple packets -dam 4/26/98 */
  //assert(rtbl_sz <= (1500 / 12)); ---how about 100++ node topologies

  p->allocdata((change_count * 9) + 1);
  walk = p->accessdata ();
  *(walk++) = change_count;

  // hdrc->size_ = change_count * 12 + 20;	// DSDV + IP
  hdrc->size_ = change_count * 12 + IP_HDR_LEN;	// DSDV + IP

  for (table_->InitLoop (); (prte = table_->NextLoop ());)
    {

      if (periodic && prte->advertise_ok_at > now)
	{ // don't send out routes that shouldn't be advertised
	  // even in periodic updates
	  continue;
	}

      if (periodic || 
	  ((prte->advert_seqnum || prte->advert_metric) 
	   && prte->advertise_ok_at <= now))
	{ // include this rte in the advert
	  if (!periodic && verbose_)
	    trace ("VCT %.5f _%d_ %d", now, myaddr_, prte->dst);

	  //assert (prte->dst < 256 && prte->metric < 256);
	  //*(walk++) = prte->dst;
	  *(walk++) = prte->dst >> 24;
 	  *(walk++) = (prte->dst >> 16) & 0xFF;
 	  *(walk++) = (prte->dst >> 8) & 0xFF;
 	  *(walk++) = (prte->dst >> 0) & 0xFF;
	  *(walk++) = prte->metric;
	  *(walk++) = (prte->seqnum) >> 24;
	  *(walk++) = ((prte->seqnum) >> 16) & 0xFF;
	  *(walk++) = ((prte->seqnum) >> 8) & 0xFF;
	  *(walk++) = (prte->seqnum) & 0xFF;

	  prte->last_advertised_metric = prte->metric;

	  // seqnum's only need to be advertised once after they change
	  prte->advert_seqnum = false; // don't need to advert seqnum again

	  if (periodic) 
	    { // a full advert means we don't have to re-advert either 
	      // metrics or seqnums again until they change
	      prte->advert_seqnum = false;
	      prte->advert_metric = false;
	    }
	  change_count--;
	}
    }  
  assert(change_count == 0);
  return p;
}

void
DSDV_Agent::updateRoute(rtable_ent *old_rte, rtable_ent *new_rte)
{
  FILE *fp = fopen("dsdv.log","a+");
  fprintf(fp, "\n\nDSDV_Agent::updateRoute\n");
  
  int negvalue = -1;
  assert(new_rte);

  Time now = Scheduler::instance().clock();

  char buf[1024];
  //  snprintf (buf, 1024, "%c %.5f _%d_ (%d,%d->%d,%d->%d,%d->%d,%lf)",
  snprintf (buf, 1024, "%c %.5f _%d_ (%d,%d->%d,%d->%d,%d->%d,%f)",
	    (new_rte->metric != BIG 
	     && (!old_rte || old_rte->metric != BIG)) ? 'D' : 'U', 
	    now, myaddr_, new_rte->dst, 
	    old_rte ? old_rte->metric : negvalue, new_rte->metric, 
	    old_rte ? old_rte->seqnum : negvalue,  new_rte->seqnum,
	    old_rte ? old_rte->hop : -1,  new_rte->hop, 
	    new_rte->advertise_ok_at);

  fprintf(fp, "new route table\n");
  fprintf(fp, "ISI: new rte metric: %d , old_rte:%d , waktu: %.12lf from %d to %d , nxthp:%d , new_rte->seqnum:%d, new_rte->hop:%d \n", 
    new_rte->metric, old_rte,now,myaddr_ , new_rte->dst, new_rte->hop,new_rte->seqnum, new_rte->hop);


  table_->AddEntry (*new_rte);
  //printf("(%d),Route table updated..\n",myaddr_);
  fprintf(fp, "(%d),MY Route table updated..\n",myaddr_);
  if (trace_wst_)
    trace ("VWST %.12lf frm %d to %d wst %.12lf nxthp %d [of %d]",
	   now, myaddr_, new_rte->dst, new_rte->wst, new_rte->hop, 
	   new_rte->metric);
  fprintf(fp, "isinya WST (settling time): %.12lf,from my_addr : %d to dest :%d, new_rte->wst %.12lf new_rte->hop %d, new_rte->metric:%d \n" , now, myaddr_ , new_rte->dst, new_rte->wst,new_rte->hop,new_rte->metric);

  if (verbose_)
    trace ("VS%s", buf);
    fprintf(fp, "Masuk Verbose --> brrti trigger update VS%s", buf);
  fprintf(fp, "ISI buf VS%s", buf);
  fclose(fp);
}

void
DSDV_Agent::processUpdate (Packet * p)
{
  FILE *fp = fopen("dsdv.log","a+");
  fprintf(fp, "\n\n DSDV_Agent::processUpdate\n");
  fprintf(fp, "ini fungsi untuk update RoutingTable\n");
  fprintf(fp, "my address %d \n", myaddr_);
  
  hdr_ip *iph = HDR_IP(p);
  Scheduler & s = Scheduler::instance ();
  double now = s.clock ();
  
  // it's a dsdv packet
  int i;
  unsigned char *d = p->accessdata ();
  unsigned char *w = d + 1;
  rtable_ent rte;		// new rte learned from update being processed
  rtable_ent *prte;		// ptr to entry *in* routing tbl

  //DEBUG
  // int src, dst;
  // src = Address::instance().print_nodeaddr(iph->src());
  // dst = Address::instance().print_nodeaddr(iph->dst());
  // //printf("Received DSDV packet from %d(%d) to %d(%d) [%d)]\n", src, iph->sport(), dst, iph->dport(), myaddr_);
  // fprintf(fp, "Received DSDV packet from %d(%d) to (%d) [%d)]\n", src, iph->sport(),  iph->dport(), myaddr_);
  fprintf(fp, "hop = iphsrc : %d \n", iph->src());
  fprintf(fp, "source port : %d \n", iph->sport());
  fprintf(fp, "dst: %d \n", iph->dst());
  fprintf(fp, "dst port : %d \n", iph->dport());

  for (i = *d; i > 0; i--)
    {
      fprintf(fp, "i ke - %d \n", i);
      bool trigger_update = false;  // do we need to do a triggered update?
      nsaddr_t dst;
      prte = NULL;

      dst = *(w++); fprintf(fp, "dst %d \n", dst);
      dst = dst << 8 | *(w++); fprintf(fp, "dst %d \n", dst);
      dst = dst << 8 | *(w++); fprintf(fp, "dst %d \n", dst);
      dst = dst << 8 | *(w++); fprintf(fp, "dst %d \n", dst);

      if ((prte = table_->GetEntry (dst)))
    	{
    	  bcopy(prte, &rte, sizeof(rte));
    	}    
      else
    	{
    	  bzero(&rte, sizeof(rte));
    	}

      rte.dst = dst; fprintf(fp, "rte.dst : %d \n", rte.dst);
      //rte.hop = iph->src();
      rte.hop = Address::instance().get_nodeaddr(iph->saddr()); fprintf(fp, "rte.hop : %d \n", rte.hop);
      rte.metric = *(w++); fprintf(fp, "rte.metric : %d \n", rte.metric);
      rte.seqnum = *(w++); fprintf(fp, "rte.seqnum : %d \n", rte.seqnum);
      rte.seqnum = rte.seqnum << 8 | *(w++); fprintf(fp, "rte.seqnum : %d \n", rte.seqnum);
      rte.seqnum = rte.seqnum << 8 | *(w++); fprintf(fp, "rte.seqnum : %d \n", rte.seqnum);
      rte.seqnum = rte.seqnum << 8 | *(w++); fprintf(fp, "rte.seqnum : %d \n", rte.seqnum);
      rte.changed_at = now; fprintf(fp, "rte.changed_at : %.5f \n", rte.changed_at);
      fprintf(fp, "Terlihat seq num nambah, tp ga melulu di update, dicount dulu bru update \n");

      if (rte.metric != BIG) rte.metric += 1; fprintf(fp, "rte.metric %d \n", rte.metric);

      if (rte.dst == myaddr_)
    	{
        fprintf(fp, "Jika rte dest == alamatku, maka :  \n");
        fprintf(fp, "rte.dst %d \n", rte.dst);
    	  if (rte.metric == BIG && periodic_callback_)
    	    {
             fprintf(fp, "Jika rte metric == BIG, maka : cancel schedule \n");
            fprintf(fp, "rte.metric %d \n", rte.metric);
    	      // You have the last word on yourself...
    	      // Tell the world right now that I'm still here....
    	      // This is a CMU Monarch optimiziation to fix the 
    	      // the problem of other nodes telling you and your neighbors
    	      // that you don't exist described in the paper.
    	      s.cancel (periodic_callback_);
    	      s.schedule (helper_, periodic_callback_, 0);
    	    }
    	  continue;		// don't corrupt your own routing table.
    	}

      /**********  fill in meta data for new route ***********/
      // If it's in the table, make it the same timeout and queue.
      fprintf(fp, "Mengisi meta data untuk rute baru \n");
      fprintf(fp, "jika ini didalam tabel, bikin itu sama dg time dan queue \n");
      if (prte)
    	{ // we already have a route to this dst
        fprintf(fp, "sebelumnya kita sudah punya rute untuk ke destinasi dst \n");
    	  if (prte->seqnum == rte.seqnum)
    	    {
            fprintf(fp, "jika seq num sebelumnya = seq num saat ini, maka : \n");
            fprintf(fp, "maka kita update tanpa seq num baru \n");
           // we've got an update with out a new squenece number
    	      // this update must have come along a different path
    	      // than the previous one, and is just the kind of thing
    	      // the weighted settling time is supposed to track.

    	      // this code is now a no-op left here for clarity -dam XXX
    	      rte.wst = prte->wst;
    	      rte.new_seqnum_at = prte->new_seqnum_at;
            fprintf(fp, "rte.wst %.5f detik ==", rte.wst);
            fprintf(fp, "prte->wst %.5f detik\n", prte->wst);
            fprintf(fp, "rte.new_seqnum_at %.5f detik\n", rte.new_seqnum_at);
            fprintf(fp, "prte->new_seqnum_at %.5f detik\n", prte->new_seqnum_at);
    	    }
    	  else 
    	    { // we've got a new seq number, end the measurement period
    	      // for wst over the course of the old sequence number
    	      // and update wst with the difference between the last
    	      // time we changed the route (which would be when the 
    	      // best route metric arrives) and the first time we heard
    	      // the sequence number that started the measurement period

    	      // do we care if we've missed a sequence number, such
    	      // that we have a wst measurement period that streches over
    	      // more than a single sequence number??? XXX -dam 4/20/98
    	      rte.wst = alpha_ * prte->wst + 
    		(1.0 - alpha_) * (prte->changed_at - prte->new_seqnum_at);
            fprintf(fp, "rte.wst %.5f detik ==", rte.wst);
    	      rte.new_seqnum_at = now;
            fprintf(fp, "rte.new_seqnum_at %.5f detik\n", rte.new_seqnum_at);
    	    }
    	}
      else
    	{ // inititallize the wst for the new route
        fprintf(fp, "Menginisialisasi settling time baru  default = 6 detik \n");
    	  rte.wst = wst0_;
    	  rte.new_seqnum_at = now;
        fprintf(fp, "rte.wst %.5f detik ==", rte.wst);
        fprintf(fp, "rte.new_seqnum_at %.5f detik\n", rte.new_seqnum_at);
    	}
	  
      // Now that we know the wst_, we know when to update...
      fprintf(fp, "Sekarang kita tau wst, kita tau kapan akan mengupdate\n");
      if (rte.metric != BIG && (!prte || prte->metric != BIG)){
        fprintf(fp, "jika rte.metric != BIG dan bukan previous rte atau metric pr rte != BIG \n");
        fprintf(fp, "rte.advertise_ok_at %.5f detik\n", rte.advertise_ok_at);
	     rte.advertise_ok_at = now + (rte.wst * 2);}
      else{
	     rte.advertise_ok_at = now;
      }

      /*********** decide whether to update our routing table *********/
      fprintf(fp, "Memutuskan apakah update routing table\n");
      if (!prte)
    	{ 
        fprintf(fp, "jika bukan previous rte\n");
        // we've heard from a brand new destination
        fprintf(fp, "Menangkap tujuan baru\n");
    	  if (rte.metric < BIG) 
    	    {
            fprintf(fp, "jika rte.metric (%d) < BIG, maka true\n",rte.metric);
    	      rte.advert_metric = true;
    	      trigger_update = true;
    	    }
    	  updateRoute(prte,&rte);
        fprintf(fp, "update rute\n");
    	}
      else if ( prte->seqnum == rte.seqnum )
	{ fprintf(fp, "jika previous seq num == rte seqnum \n");
    // stnd dist vector case
    fprintf(fp, "standart distance vektor\n");
	  if (rte.metric < prte->metric) 
	    { // a shorter route! 
        fprintf(fp, "rute yg lebih pendek\n");
	      if (rte.metric == prte->last_advertised_metric)
		{ // we've just gone back to a metric we've already advertised
      fprintf(fp, "kami baru saja kembali ke metric yg telah kami broadcast\n");
		  rte.advert_metric = false;
		  trigger_update = false;
		}
	      else
		{ // we're changing away from the last metric we announced
      fprintf(fp, "kami beralih ke metric yg terakhir kami broadcast\n");
		  rte.advert_metric = true;
		  trigger_update = true;
		}
	      updateRoute(prte,&rte);
	    }
	  else
	    { // ignore the longer route
        fprintf(fp, "abaikan rute yg panjang\n");
	    }
	}
      else if ( prte->seqnum < rte.seqnum )
	{ // we've heard a fresher sequence number
	  // we *must* believe its rt metric
	  rte.advert_seqnum = true;	// we've got a new seqnum to advert
    fprintf(fp, "Kami mendapat seq num baru untuk di broadcast \n");
	  if (rte.metric == prte->last_advertised_metric)
	    { // we've just gone back to our old metric
        fprintf(fp, "Kami menggunakan metric kami yg lama\n");
	      rte.advert_metric = false;
	    }
	  else
	    { // we're using a metric different from our last advert
        fprintf(fp, "Kami menggunakan metric yg berbeda dr trakhir broadcast \n");
	      rte.advert_metric = true;
	    }

	  updateRoute(prte,&rte); 
    fprintf(fp, "update rute\n");


#ifdef TRIGGER_UPDATE_ON_FRESH_SEQNUM
	  trigger_update = true;
#else
	  trigger_update = false;
#endif
	}
      else if ( prte->seqnum > rte.seqnum )
	{ // our neighbor has older sequnum info than we do
	  if (rte.metric == BIG && prte->metric != BIG)
	    { // we must go forth and educate this ignorant fellow
	      // about our more glorious and happy metric
	      prte->advertise_ok_at = now;
	      prte->advert_metric = true;
	      // directly schedule a triggered update now for 
	      // prte, since the other logic only works with rte.*
	      needTriggeredUpdate(prte,now);
	    }
	  else
	    { // we don't care about their stale info
	    }
	}
      else
	{
	  fprintf(stderr,
		  "%s DFU: unhandled adding a route entry?\n", __FILE__);
	  abort();
	}
      
      if (trigger_update)
	{
	  prte = table_->GetEntry (rte.dst);
	  assert(prte != NULL && prte->advertise_ok_at == rte.advertise_ok_at);
	  needTriggeredUpdate(prte, prte->advertise_ok_at);
	}

      // see if we can send off any packets we've got queued
      if (rte.q && rte.metric != BIG)
	{
	  Packet *queued_p;
	  while ((queued_p = rte.q->deque()))
	  // XXX possible loop here  
	  // while ((queued_p = rte.q->deque()))
	  // Only retry once to avoid looping
	  // for (int jj = 0; jj < rte.q->length(); jj++){
	  //  queued_p = rte.q->deque();
	    recv(queued_p, 0); // give the packets to ourselves to forward
      fprintf(fp, "recv : give the packets to ourselves to forward\n");
	  // }
	  delete rte.q;
	  rte.q = 0;
	  table_->AddEntry(rte); // record the now zero'd queue
	}
    } // end of all destination mentioned in routing update packet

  // Reschedule the timeout for this neighbor
  prte = table_->GetEntry(Address::instance().get_nodeaddr(iph->saddr()));
  if (prte)
    {
      if (prte->timeout_event)
	s.cancel (prte->timeout_event);
      else
	{
	  prte->timeout_event = new Event ();
	}
      
      s.schedule (helper_, prte->timeout_event, min_update_periods_ * perup_);
    }
  else
    { // If the first thing we hear from a node is a triggered update
      // that doesn't list the node sending the update as the first
      // thing in the packet (which is disrecommended by the paper)
      // we won't have a route to that node already.  In order to timeout
      // the routes we just learned, we need a harmless route to keep the
      // timeout metadata straight.
      
      // Hi there, nice to meet you. I'll make a fake advertisement
      bzero(&rte, sizeof(rte));
      rte.dst = Address::instance().get_nodeaddr(iph->saddr());
      rte.hop = Address::instance().get_nodeaddr(iph->saddr());
      rte.metric = 1;
      rte.seqnum = 0;
      rte.advertise_ok_at = now + 604800;	// check back next week... :)
      
      rte.changed_at = now;
      rte.new_seqnum_at = now;
      rte.wst = wst0_;
      rte.timeout_event = new Event ();
      rte.q = 0;
      
      updateRoute(NULL, &rte);
      s.schedule(helper_, rte.timeout_event, min_update_periods_ * perup_);
    }
  
  /*
   * Freeing a routing layer packet --> don't need to
   * call drop here.
   */
  Packet::free (p);

  fclose(fp);

}

int 
DSDV_Agent::diff_subnet(int dst) 
{
	char* dstnet = Address::instance().get_subnetaddr(dst);
	if (subnet_ != NULL) {
		if (dstnet != NULL) {
			if (strcmp(dstnet, subnet_) != 0) {
				delete [] dstnet;
				return 1;
			}
			delete [] dstnet;
		}
	}
	//assert(dstnet == NULL);
	return 0;
}



void
DSDV_Agent::forwardPacket (Packet * p)
{
  FILE *fp = fopen("dsdv.log","a+");
  fprintf(fp, "\n\nDSDV_Agent::forwardPacket\n");
  fprintf(fp, "ini fungsi forward paket \n");
  hdr_ip *iph = HDR_IP(p);
  Scheduler & s = Scheduler::instance ();
  double now = s.clock ();
  hdr_cmn *hdrc = HDR_CMN (p);
  int dst;
  rtable_ent *prte;

  double xpos;
  double ypos;
  double zpos;
  double speed;
  double radius;
  
  // We should route it.
  //printf("(%d)-->forwardig pkt\n",myaddr_);
  fprintf(fp, "sebaiknya merutekan itu \n");
  fprintf(fp, "(%d)-->forwardig pkt\n", myaddr_);

  xpos = node_->X();
  ypos = node_->Y();
  zpos = node_->Z();
  speed = node_->speed();
  radius = node_->radius();
  fprintf(fp, "x= %f, y = %f, z= %f \n", xpos, ypos, zpos);
  fprintf(fp, "Speed : %f radius: %f \n", speed, radius);
  // set direction of pkt to -1 , i.e downward
  fprintf(fp, "DISINI ADA direction, set direction to downward \n");
  hdrc->direction() = hdr_cmn::DOWN;

  // if the destination is outside mobilenode's domain
  // forward it to base_stn node
  // Note: pkt is not buffered if route to base_stn is unknown

  fprintf(fp, "Jika destination diluar domain node, forward ke base station node, paket tidak di buffer jika rute ke base station tidak diketahui \n");

  dst = Address::instance().get_nodeaddr(iph->daddr());
  fprintf(fp, "destination : %d \n",dst);  
  if (diff_subnet(iph->daddr())) {
	   prte = table_->GetEntry (dst);
	  if (prte && prte->metric != BIG)
    {
      goto send;
      fprintf(fp, "jika ada di previous rte dan metric previous rte tidak BIG maka kirim \n");
    } 
		  
	  
	  //int dst = (node_->base_stn())->address();
	  dst = node_->base_stn();
    
    fprintf(fp, "BASE station : %d \n",dst);
	  prte = table_->GetEntry (dst);
	  if (prte && prte->metric != BIG) 
    {
		  goto send;
      fprintf(fp, "kirim ke base_stn \n");
    }
	  
	  else {
		  //drop pkt with warning
		  fprintf(stderr, "warning: Route to base_stn not known: dropping pkt\n");
      fprintf(fp, "drop paket dg warning \n");
		  Packet::free(p);
		  return;
	  }
  }
  
  prte = table_->GetEntry (dst);
  
  //  trace("VDEBUG-RX %d %d->%d %d %d 0x%08x 0x%08x %d %d", 
  //  myaddr_, iph->src(), iph->dst(), hdrc->next_hop_, hdrc->addr_type_,
  //  hdrc->xmit_failure_, hdrc->xmit_failure_data_,
  //  hdrc->num_forwards_, hdrc->opt_num_forwards);

  if (prte && prte->metric != BIG)
    {
       //printf("(%d)-have route for dst\n",myaddr_);
      fprintf(fp, "(%d)-have route for dst %d \n",myaddr_ , dst);
       goto send;
    }
  else if (prte)
    { /* must queue the packet */
	    //printf("(%d)-no route, queue pkt\n",myaddr_);
      fprintf(fp, "(%d)-no route ke %d, queue pkt\n",myaddr_ , dst);
      if (!prte->q)
	{
	  prte->q = new PacketQueue ();
	}

      prte->q->enque(p);

      if (verbose_){
        fprintf(fp, "Disini Masuk verbose \n");
        trace ("VBP %.5f _%d_ %d:%d -> %d:%d", now, myaddr_, iph->saddr(),
         iph->sport(), iph->daddr(), iph->dport());
        fprintf(fp, "VBP now: %.5f myaddr:_%d_ source address:%d source port:%d -> destination address%d destination port:%d", now, myaddr_, iph->saddr(),
         iph->sport(), iph->daddr(), iph->dport());
      }
	
      while (prte->q->length () > MAX_QUEUE_LENGTH)
	      drop (prte->q->deque (), DROP_RTR_QFULL);
      return;
    }
  else
    { // Brand new destination
      fprintf(fp, "Brand new destination \n");
      rtable_ent rte;
      double now = s.clock();
      
      bzero(&rte, sizeof(rte));
      rte.dst = dst;
      rte.hop = dst;
      rte.metric = BIG;
      rte.seqnum = 0;

      rte.advertise_ok_at = now + 604800;	// check back next week... :)
      rte.changed_at = now;
      rte.new_seqnum_at = now;	// was now + wst0_, why??? XXX -dam
      rte.wst = wst0_;
      rte.timeout_event = 0;

      fprintf(fp, "rte.dst: %d, rte.hop : %d, rte.metric: %d,rte.seqnum : %d,rte.advertise_ok_at : %.5f,rte.changed_at: %.5f ,rte.new_seqnum_at: %.5f ,rte.wst : %.5f ,rte.timeout_event : %.5f \n",rte.dst ,rte.hop ,rte.metric,rte.seqnum ,rte.advertise_ok_at ,rte.changed_at ,rte.new_seqnum_at ,rte.wst ,rte.timeout_event );

      rte.q = new PacketQueue();
      rte.q->enque(p); //memasukkan ke queue
	  
      assert (rte.q->length() == 1 && 1 <= MAX_QUEUE_LENGTH);
      table_->AddEntry(rte);
      
      if (verbose_){
	      trace ("VBP %.5f _%d_ %d:%d -> %d:%d", now, myaddr_,
		     iph->saddr(), iph->sport(), iph->daddr(), iph->dport());
        fprintf(fp, "Masuk verbose \n" );
        fprintf(fp, "VBP %.5f _%d_ %d:%d -> %d:%d", now, myaddr_,
         iph->saddr(), iph->sport(), iph->daddr(), iph->dport());
      }
      return;
    }


 send:
  fprintf(fp, "Disini fungsi send \n");
  hdrc->addr_type_ = NS_AF_INET; fprintf(fp, "hdrc->addr_type_ :%d\n",hdrc->addr_type_  );
  hdrc->xmit_failure_ = mac_callback; fprintf(fp, "hdrc->xmit_failure_: %d\n", hdrc->xmit_failure_);
  hdrc->xmit_failure_data_ = this; fprintf(fp, "hdrc->xmit_failure_data_ %d\n", hdrc->xmit_failure_data_);
  if (prte->metric > 1){
	  hdrc->next_hop_ = prte->hop;
    fprintf(fp, "Jika metric previous rte lebih dr 1 maka : next_hop_ = %d \n",hdrc->next_hop_ );
  }
  else{
	  hdrc->next_hop_ = dst;
    fprintf(fp, "Else maka : next_hop_ = %d \n",hdrc->next_hop_ );
  }
  if (verbose_){
	  trace ("Routing pkts outside domain: VFP %.5f _%d_ %d:%d -> %d:%d", now, myaddr_, iph->saddr(),
		 iph->sport(), iph->daddr(), iph->dport());
     fprintf(fp, "Routing pkts outside domain: VFP %.5f _%d_ %d:%d -> %d:%d", now, myaddr_, iph->saddr(),
     iph->sport(), iph->daddr(), iph->dport());  
    }
  assert (!HDR_CMN (p)->xmit_failure_ ||
	  HDR_CMN (p)->xmit_failure_ == mac_callback);
  target_->recv(p, (Handler *)0);
  fprintf(fp, "Target received paket\n" );
  return;
  fclose(fp);
}

void 
DSDV_Agent::sendOutBCastPkt(Packet *p)
{
  FILE *fp = fopen("dsdv.log","a+");
  fprintf(fp, "DSDV_Agent::sendOutBCastPkt\n");
  fprintf(fp, "ini fungsi broadcast paket\n");
  
  Scheduler & s = Scheduler::instance ();
  // send out bcast pkt with jitter to avoid sync
  s.schedule (target_, p, jitter(DSDV_BROADCAST_JITTER, be_random_));
  fclose(fp);
}


void
DSDV_Agent::recv (Packet * p, Handler *)
{
  FILE *fp = fopen("dsdv.log","a+");
  fprintf(fp, "\n\nDSDV_Agent::recv\n");
  fprintf(fp, "fungsi ketika menerima paket \n");
  float xpos = node_->X();
  float ypos = node_->Y();
  float speed = node_->speed();
  float radius = node_->radius();
  
  fprintf(fp, "x= %f, y = %f \n", xpos, ypos);
  fprintf(fp, "Speed : %.10f radius: %f \n", speed, radius);
  fprintf(fp, "perup umum %.5f \n",  perup_ );

  
  hdr_ip *iph = HDR_IP(p);
  hdr_cmn *cmh = HDR_CMN(p);

  int src = Address::instance().get_nodeaddr(iph->saddr());
  int dest2 = Address::instance().get_nodeaddr(iph->daddr());
  /*
   *  Must be a packet I'm originating...
   */
  if(src == myaddr_ && cmh->num_forwards() == 0) {
    /*
     * Add the IP Header
     */
    fprintf(fp, "jika src = %d adalah ipku %d dan forward \n", src, myaddr_);
    fprintf(fp, "Add the IP header\n");
    cmh->size() += IP_HDR_LEN;    
    iph->ttl_ = IP_DEF_TTL;
    fprintf(fp, "TTL %d \n", iph->ttl_);
  }
  /*
   *  I received a packet that I sent.  Probably
   *  a routing loop.
   */
  else if(src == myaddr_) {
    fprintf(fp, "jika src == my addr atau bisa jg disebut menerima paket yg bru saja saya kirim, ini mungkin terjadi loop route maka drop\n");
    drop(p, DROP_RTR_ROUTE_LOOP);
    return;
  }
  /*
   *  Packet I'm forwarding...
   */
  else {
    /*
     *  Check the TTL.  If it is zero, then discard.
     */
    fprintf(fp, "Saya for paket \n");
    fprintf(fp, "my addr : %d source : %d \n",myaddr_, src);
    fprintf(fp, "Cek ttl = : %d \n", iph->ttl_ );
    if(--iph->ttl_ == 0) {
      drop(p, DROP_RTR_TTL);
      fprintf(fp, "Cek TTL , kalo ttl 0 , maka drop. Berarti tidak menemukan rute \n");
      return;
    }
  }
  
  if ((src != myaddr_) && (iph->dport() == ROUTER_PORT))
    {
      fprintf(fp, "kalo sumbernya bukan dr ipku (%d) dan port di ip headernya = router port maka update paket \n", myaddr_);
      fprintf(fp, "source : %d:%d \n", src, iph->dport() );
	    // XXX disable this feature for mobileIP where
	    // the MH and FA (belonging to diff domains)
	    // communicate with each other.

	    // Drop pkt if rtg update from some other domain:
	    // if (diff_subnet(iph->src())) 
	    // drop(p, DROP_OUTSIDE_SUBNET);
	    //else    
	    processUpdate(p);
      fprintf(fp, "update rute \n");
    }
  else if (iph->daddr() == ((int)IP_BROADCAST) &&
	   (iph->dport() != ROUTER_PORT)) 
	  {
      fprintf(fp, "dest : %d:%d \n", dest2, iph->dport() );
	     if (src == myaddr_) {
        fprintf(fp, "dest : %d:%d \n", dest2, iph->dport() );
		     // handle brdcast pkt
		     sendOutBCastPkt(p);
         fprintf(fp, "broadcast paket \n");
	     }
	     else {
		     // hand it over to the port-demux
		    port_dmux_->recv(p, (Handler*)0);
        fprintf(fp, "receive\n");
	     }
	  }
  else 
    {
	    forwardPacket(p);
      fprintf(fp, "forward paket \n");
    }

    fclose(fp);
}

static class DSDVClass:public TclClass
{
  public:
  DSDVClass ():TclClass ("Agent/DSDV")
  {
  }
  TclObject *create (int, const char *const *)
  {
    return (new DSDV_Agent ());
  }
} class_dsdv;

// setiap DSDV agent punya atribut ini

DSDV_Agent::DSDV_Agent (): Agent (PT_MESSAGE), ll_queue (0), seqno_ (0), 
  myaddr_ (0), subnet_ (0), node_ (0), port_dmux_(0),
  periodic_callback_ (0), be_random_ (1), 
  use_mac_ (0), verbose_ (1), trace_wst_ (0), lasttup_ (-10), 
  alpha_ (0.875),  wst0_ (6), perup_ (15), 
  min_update_periods_ (3)	// constants
 {
  table_ = new RoutingTable ();
  helper_ = new DSDV_Helper (this);
  trigger_handler = new DSDVTriggerHandler(this);

  bind_time ("wst0_", &wst0_);
  bind_time ("perup_", &perup_);

  bind ("use_mac_", &use_mac_);
  bind ("be_random_", &be_random_);
  bind ("alpha_", &alpha_);
  bind ("min_update_periods_", &min_update_periods_);
  bind ("verbose_", &verbose_);
  bind ("trace_wst_", &trace_wst_);
  //DEBUG
  address = 0;
}

void
DSDV_Agent::startUp()
{
  FILE *fp = fopen("dsdv.log","a+");
  fprintf(fp, "\n \n DSDV_Agent::startUp \n");
  
  Time now = Scheduler::instance().clock();

  subnet_ = Address::instance().get_subnetaddr(myaddr_);
  //DEBUG

  address = Address::instance().print_nodeaddr(myaddr_);
  //printf("myaddress: %d -> %s\n",myaddr_,address);

  double xpos;
  double ypos;
  double speed;
  double radius;
  
  
  xpos = node_->X();
  ypos = node_->Y();
  speed = node_->speed();
  radius = node_->radius();
  
  fprintf(fp, "x= %f, y = %f \n", xpos, ypos);
  fprintf(fp, "Speed : %f radius: %f \n", speed, radius);
  fprintf(fp, "perup umum %.5f \n",  perup_ );


  fprintf(fp, "time now = %.5f \n", now);
  fprintf(fp, "myaddr: %d -> address : %s \n",myaddr_,address); // alamat nodenya 
  
  rtable_ent rte;
  bzero(&rte, sizeof(rte)); // sama dengan memset, sebesar ukuran routing table
  fprintf(fp, "size routing table: %ld \n",sizeof(rte));
  //fprintf(fp, "perup my addr %.5f \n",  rte.perup_ );


  // mengisi routing table
  rte.dst = myaddr_;
  rte.hop = myaddr_;
  rte.metric = 0;
  rte.seqnum = seqno_;
  fprintf(fp, "rte dest : %d, rte.hop: %d , rte sequence number: %d , metric : %d \n",rte.dst,rte.hop, rte.seqnum, rte.metric);
  seqno_ += 2;
  fprintf(fp, "setelah +2: %d \n",seqno_ );

  rte.advertise_ok_at = 0.0; // can always advert ourselves
  rte.advert_seqnum = true;
  rte.advert_metric = true;
  rte.changed_at = now;
  rte.new_seqnum_at = now;
  rte.wst = 0;
  rte.modif_perup = 0;
  rte.modif_minupdateperup =0;
  rte.timeout_event = 0;		// Don't time out our localhost :)
  
  rte.q = 0;		// Don't buffer pkts for self!
  
  table_->AddEntry (rte); //memasukkan ke tabel
  fprintf(fp, " rte.dst =%d , rte.hop=%d ,rte.metric=%d ,rte.seqnum=%d ,rte.advertise_ok_at=%.5f ,rte.advert_seqnum=%d ,rte.advert_metric=%d ,rte.changed_at=%.5f ,rte.new_seqnum_at= %.5f , rte.wst= %d , rte.timeout_event = %d \n",
    rte.dst, rte.hop, rte.metric,rte.seqnum,rte.advertise_ok_at,rte.advert_seqnum, rte.advert_metric,rte.changed_at, rte.new_seqnum_at, rte.wst, rte.timeout_event);

  fprintf(fp, "kick off periodic advertisments \n");
  // kick off periodic advertisments
  periodic_callback_ = new Event ();
  Scheduler::instance ().schedule (helper_, 
				   periodic_callback_,
				   jitter (DSDV_STARTUP_JITTER, be_random_));

  fclose(fp);
}

int 
DSDV_Agent::command (int argc, const char *const *argv)
{
  if (argc == 2)
    {
      if (strcmp (argv[1], "start-dsdv") == 0)
	{
	  startUp();
	  return (TCL_OK);
	}
      else if (strcmp (argv[1], "dumprtab") == 0)
	{
	  Packet *p2 = allocpkt ();
	  hdr_ip *iph2 = HDR_IP(p2);
	  rtable_ent *prte;

	  printf ("Table Dump %d[%d]\n----------------------------------\n",
		  iph2->saddr(), iph2->sport());
	trace ("VTD %.5f %d:%d\n", Scheduler::instance ().clock (),
		 iph2->saddr(), iph2->sport());

	  /*
	   * Freeing a routing layer packet --> don't need to
	   * call drop here.
	   */
	Packet::free (p2);

	  for (table_->InitLoop (); (prte = table_->NextLoop ());)
	    output_rte ("\t", prte, this);

	  printf ("\n");

	  return (TCL_OK);
	}
      else if (strcasecmp (argv[1], "ll-queue") == 0)
	{
	if (!(ll_queue = (PriQueue *) TclObject::lookup (argv[2])))
	    {
	      fprintf (stderr, "DSDV_Agent: ll-queue lookup of %s failed\n", argv[2]);
	      return TCL_ERROR;
	    }

	  return TCL_OK;
	}

    }
  else if (argc == 3)
    {
      if (strcasecmp (argv[1], "addr") == 0) {
	 int temp;
	 temp = Address::instance().str2addr(argv[2]);
	 myaddr_ = temp;
	 return TCL_OK;
      }
      TclObject *obj;
      if ((obj = TclObject::lookup (argv[2])) == 0)
	{
	  fprintf (stderr, "%s: %s lookup of %s failed\n", __FILE__, argv[1],
		   argv[2]);
	  return TCL_ERROR;
	}
      if (strcasecmp (argv[1], "tracetarget") == 0)
	{
	  
	  tracetarget = (Trace *) obj;
	  return TCL_OK;
	}
      else if (strcasecmp (argv[1], "node") == 0) {
	      node_ = (MobileNode*) obj;
	      return TCL_OK;
      }
      else if (strcasecmp (argv[1], "port-dmux") == 0) {
	      port_dmux_ = (NsObject *) obj;
	      return TCL_OK;
      }
    }
  
  return (Agent::command (argc, argv));
}




