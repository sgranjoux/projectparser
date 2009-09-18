
#ifndef __libanjuta_iface_cclosure_marshal_MARSHAL_H__
#define __libanjuta_iface_cclosure_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* VOID:BOOLEAN,INT (./libanjuta-iface-marshallers.list:1) */
extern void libanjuta_iface_cclosure_marshal_VOID__BOOLEAN_INT (GClosure     *closure,
                                                                GValue       *return_value,
                                                                guint         n_param_values,
                                                                const GValue *param_values,
                                                                gpointer      invocation_hint,
                                                                gpointer      marshal_data);

/* VOID:BOXED (./libanjuta-iface-marshallers.list:2) */
#define libanjuta_iface_cclosure_marshal_VOID__BOXED	g_cclosure_marshal_VOID__BOXED

/* VOID:ENUM (./libanjuta-iface-marshallers.list:3) */
#define libanjuta_iface_cclosure_marshal_VOID__ENUM	g_cclosure_marshal_VOID__ENUM

/* VOID:INT (./libanjuta-iface-marshallers.list:4) */
#define libanjuta_iface_cclosure_marshal_VOID__INT	g_cclosure_marshal_VOID__INT

/* VOID:INT,INT (./libanjuta-iface-marshallers.list:5) */
extern void libanjuta_iface_cclosure_marshal_VOID__INT_INT (GClosure     *closure,
                                                            GValue       *return_value,
                                                            guint         n_param_values,
                                                            const GValue *param_values,
                                                            gpointer      invocation_hint,
                                                            gpointer      marshal_data);

/* VOID:INT,INT,ULONG,STRING,UINT (./libanjuta-iface-marshallers.list:6) */
extern void libanjuta_iface_cclosure_marshal_VOID__INT_INT_ULONG_STRING_UINT (GClosure     *closure,
                                                                              GValue       *return_value,
                                                                              guint         n_param_values,
                                                                              const GValue *param_values,
                                                                              gpointer      invocation_hint,
                                                                              gpointer      marshal_data);

/* VOID:OBJECT (./libanjuta-iface-marshallers.list:7) */
#define libanjuta_iface_cclosure_marshal_VOID__OBJECT	g_cclosure_marshal_VOID__OBJECT

/* VOID:OBJECT,BOOLEAN,INT,INT,STRING (./libanjuta-iface-marshallers.list:8) */
extern void libanjuta_iface_cclosure_marshal_VOID__OBJECT_BOOLEAN_INT_INT_STRING (GClosure     *closure,
                                                                                  GValue       *return_value,
                                                                                  guint         n_param_values,
                                                                                  const GValue *param_values,
                                                                                  gpointer      invocation_hint,
                                                                                  gpointer      marshal_data);

/* VOID:OBJECT,CHAR (./libanjuta-iface-marshallers.list:9) */
extern void libanjuta_iface_cclosure_marshal_VOID__OBJECT_CHAR (GClosure     *closure,
                                                                GValue       *return_value,
                                                                guint         n_param_values,
                                                                const GValue *param_values,
                                                                gpointer      invocation_hint,
                                                                gpointer      marshal_data);

/* VOID:POINTER (./libanjuta-iface-marshallers.list:10) */
#define libanjuta_iface_cclosure_marshal_VOID__POINTER	g_cclosure_marshal_VOID__POINTER

/* VOID:STRING (./libanjuta-iface-marshallers.list:11) */
#define libanjuta_iface_cclosure_marshal_VOID__STRING	g_cclosure_marshal_VOID__STRING

/* VOID:STRING,STRING (./libanjuta-iface-marshallers.list:12) */
extern void libanjuta_iface_cclosure_marshal_VOID__STRING_STRING (GClosure     *closure,
                                                                  GValue       *return_value,
                                                                  guint         n_param_values,
                                                                  const GValue *param_values,
                                                                  gpointer      invocation_hint,
                                                                  gpointer      marshal_data);

/* VOID:UINT,INT (./libanjuta-iface-marshallers.list:13) */
extern void libanjuta_iface_cclosure_marshal_VOID__UINT_INT (GClosure     *closure,
                                                             GValue       *return_value,
                                                             guint         n_param_values,
                                                             const GValue *param_values,
                                                             gpointer      invocation_hint,
                                                             gpointer      marshal_data);

/* VOID:ULONG,STRING,UINT (./libanjuta-iface-marshallers.list:14) */
extern void libanjuta_iface_cclosure_marshal_VOID__ULONG_STRING_UINT (GClosure     *closure,
                                                                      GValue       *return_value,
                                                                      guint         n_param_values,
                                                                      const GValue *param_values,
                                                                      gpointer      invocation_hint,
                                                                      gpointer      marshal_data);

/* VOID:VOID (./libanjuta-iface-marshallers.list:15) */
#define libanjuta_iface_cclosure_marshal_VOID__VOID	g_cclosure_marshal_VOID__VOID

G_END_DECLS

#endif /* __libanjuta_iface_cclosure_marshal_MARSHAL_H__ */

