#ifndef XQ_CONTROL_H
#define XQ_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register all Modbus control handlers
 * @note Should be called during system initialization
 */
void xq_register_all_handlers(void);

void xq_update_axis_status(void);

#ifdef __cplusplus
}
#endif

#endif // XQ_CONTROL_H
