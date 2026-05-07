#include "updates/otw_packet_parser.h"
#include <string.h>

static struct otw_packet_parser_t *_local_packet_parser;

static void timer_reset(struct otw_packet_parser_t *pp) {
  __HAL_TIM_SET_COUNTER(pp->timer, 0);
  HAL_TIM_Base_Start_IT(pp->timer);
}

static void timer_stop(struct otw_packet_parser_t *pp) {
  HAL_TIM_Base_Stop_IT(pp->timer);
  __HAL_TIM_SET_COUNTER(pp->timer, 0);
}

void otw_packet_parser_init(struct otw_packet_parser_t *pp,
                            struct ring_buffer_t *rb, TIM_HandleTypeDef *tim) {
  pp->rb = rb;
  pp->timer = tim;

  _local_packet_parser = pp;

  otw_packet_parser_reset(pp);
}

void otw_packet_parser_reset(struct otw_packet_parser_t *pp) {
  /* TODO:  not sure if this will reset it nicely */
  struct ring_buffer_t *rb = pp->rb;
  TIM_HandleTypeDef *timer = pp->timer;

  *pp = (struct otw_packet_parser_t){
      .rb = rb,
      .timer = timer,
      .state = OTW_PARSE_STATE_CMD,
  };

  timer_stop(pp);
}

void otw_packet_parser_timeout_callback(struct otw_packet_parser_t *pp,
                                        TIM_HandleTypeDef *tim) {
  if (tim->Instance != _local_packet_parser->timer->Instance) {
    return;
  }

  timer_stop(pp);
  pp->timeout = true;
}
enum otw_parse_result_t
otw_packet_parser_update(struct otw_packet_parser_t *pp) {
  
  if (pp->timeout) {
    return OTW_PARSE_RESULT_ERROR;
  }

  uint8_t byte;
  while (rb_get(pp->rb, &byte)) {
    if (!pp->receiving) {
      pp->receiving = true;
    }
    /* new byte, reset the timer */
    //timer_reset(pp);

    switch (pp->state) {
    case OTW_PARSE_STATE_CMD: {
      pp->packet.cmd = byte;
      pp->state = OTW_PARSE_STATE_LEN_LOW;
      break;
    }
    case OTW_PARSE_STATE_LEN_LOW: {
      pp->packet.length = byte;
      pp->state = OTW_PARSE_STATE_LEN_HIGH;
      break;
    }
    case OTW_PARSE_STATE_LEN_HIGH: {
      pp->packet.length |= ((uint16_t)byte << 8);
      if (pp->packet.length > MAX_PAYLOAD_SIZE)
        return OTW_PARSE_RESULT_ERROR;

      if (pp->packet.length == 0) {
        pp->crc_idx = 0;
        pp->state = OTW_PARSE_STATE_CRC;
      } else {
        pp->payload_idx = 0;
        pp->state = OTW_PARSE_STATE_PAYLOAD;
      }
      break;
    }
    case OTW_PARSE_STATE_PAYLOAD: {
      pp->packet.payload[pp->payload_idx++] = byte;
      if (pp->payload_idx >= pp->packet.length) {
        pp->crc_idx = 0;
        pp->state = OTW_PARSE_STATE_CRC;
      }
      break;
    }
    case OTW_PARSE_STATE_CRC: {
      pp->crc_buf[pp->crc_idx++] = byte;
      if (pp->crc_idx >= 4) {
        pp->packet.crc =
            ((uint32_t)pp->crc_buf[0]) | ((uint32_t)pp->crc_buf[1] << 8) |
            ((uint32_t)pp->crc_buf[2] << 16) | ((uint32_t)pp->crc_buf[3] << 24);
        timer_stop(pp);
        return OTW_PARSE_RESULT_COMPLETE;
      }
      break;
    }
    }
  }

  return OTW_PARSE_RESULT_BUSY;
}

const struct otw_uart_packet_t *
otw_packet_parser_get_packet(const struct otw_packet_parser_t *pp) {
  return &pp->packet;
}
