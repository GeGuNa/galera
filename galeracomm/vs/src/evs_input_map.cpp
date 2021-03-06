
#include "evs_input_map.hpp"


EVSRange EVSInputMap::insert(const EVSInputMapItem& item)
{
    // Message can be either USER or LEAVE message
    assert(item.get_evs_message().get_type() == EVSMessage::USER);
    
    // Find corresponding instance
    IMap::iterator ii = instances.find(item.get_sockaddr());
    if (ii == instances.end())
        throw FatalException("Instance not found");
    
    // 
    EVSRange& gap(ii->second.gap);
    // MEssage seqno
    uint32_t seq = item.get_evs_message().get_seq();
    // MEssage seqno range 
    uint8_t seq_range = item.get_evs_message().get_seq_range();        
    // Starting point of the allowed seqno window
    uint32_t wseq = seqno_eq(aru_seq, SEQNO_MAX) ? 0 : aru_seq;

    // Check whether this message is inside allowed seqno windon
    if (seqno_gt(seq, seqno_add(wseq, SEQNO_MAX/4)) ||
        seqno_lt(seq, seqno_dec(wseq, SEQNO_MAX/4))) {
        LOG_WARN(std::string("Seqno out of window: ") + 
                 to_string(seq) + " current aru " + to_string(aru_seq));
        return EVSRange(gap);
    }
        
    // If gap.high is SEQNO_MAX, then also gap.low must be SEQNO_MAX
    assert(!seqno_eq(gap.high, SEQNO_MAX) || seqno_eq(gap.low, SEQNO_MAX));

    // Iterator over seqno range contained in message. Insert separate 
    // entry corresponding to each seqno, assign payload only to the 
    // beginning of the range. 
    for (uint32_t i = seq; !seqno_gt(i, seqno_add(seq, seq_range)); 
         i = seqno_next(i)) {

        std::pair<iterator, bool> iret;

        if (seqno_eq(i, seq)) {
            // The first message of the range
            ReadBuf* new_rb = item.get_readbuf() ? item.get_readbuf()->copy(
                item.get_readbuf_offset()) : 0;
            iret = msg_log.insert(
                EVSInputMapItem(item.get_sockaddr(), 
                                item.get_evs_message(),
                                new_rb));
            // Message could not be inserted, release newly allocated
            // readbuf
            if (iret.second == false && new_rb)
                new_rb->release();
        } else {
            // Insert empty placeholder 
            iret = msg_log.insert(
                EVSInputMapItem(
                    item.get_sockaddr(),
                    EVSMessage(EVSMessage::USER,
                               item.get_evs_message().get_source(),
                               DROP,
                               i, 0, 
                               item.get_evs_message().get_aru_seq(),
                               item.get_evs_message().get_source_view(), 
                               0), 
                    0));
        }


        if (iret.second) {
            // New message was inserted
            
            // Adjust gap high
            if (seqno_eq(gap.high, SEQNO_MAX) || seqno_gt(i, gap.high)) 
                gap.high = i;
            
            // Adjust gap low
            if ((seqno_eq(gap.low, SEQNO_MAX) && seqno_eq(i, 0)) || 
                seqno_eq(i, gap.low)) {
                gap.low = seqno_eq(gap.low, SEQNO_MAX) ? 1 : seqno_next(gap.low);

                if (!seqno_gt(gap.low, gap.high)) {
                    // Gap low is less than equal to high indicates that there
                    // have possibly been missing messages that have been 
                    // recovered. 
                    MLog::iterator mi = iret.first;
                    for (++mi; mi != msg_log.end(); ++mi) {
                        // Yes, this is not optimal, but this should
                        // be quite rare routine under considerably 
                        // small message loss
                        LOG_TRACE(std::string("\t") +
                                  to_string(mi->get_evs_message().get_seq()) + " " + to_string(gap.low));
                        if (mi->get_sockaddr() == item.get_sockaddr()) {
                            if (seqno_eq(mi->get_evs_message().get_seq(), 
                                         gap.low)) {
                                gap.low = seqno_next(gap.low);
                            } else {
                                // Found hole in message sequence
                                assert(seqno_gt(mi->get_evs_message().get_seq(),
                                                gap.low));
                                break;
                            }
                        }
                        LOG_TRACE(std::string("\t") + to_string(gap.low));
                    }
                }
            } 
            n_messages++;
            msg_log_size_cum += msg_log.size();
        } else {
            // TODO: Sanity check, verify that this message matches to 
            // the message already in map
        }
    }
    
#if 0
    for (MLog::iterator i = msg_log.begin(); i != msg_log.end(); ++i) {
        LOG_TRACE(std::string("MLog: ") + i->get_sockaddr().to_string() + " " + to_string(i->get_evs_message().get_seq()));
    }
#endif 
    LOG_TRACE(std::string("EVSInputMap::insert(): ") 
              + " aru_seq = " + to_string(aru_seq)
              + " safe_seq = " + to_string(safe_seq)
              + " low = " + to_string(gap.low)
              + " high = " + to_string(gap.high));

    if (seqno_eq(aru_seq, SEQNO_MAX) || seqno_gt(gap.low, seqno_next(aru_seq)))
        update_aru();
    
    if (!seqno_eq(item.get_evs_message().get_aru_seq(), SEQNO_MAX))
        set_safe(ii->first, item.get_evs_message().get_aru_seq());

    return EVSRange(gap);
}
