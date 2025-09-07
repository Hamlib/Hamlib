/*
Common SWIG macros
*/

/*
 * Macro GENERATE_HAS_GET_SET
 *
 * Creates two methods *_has_get_*() and *_has_set_*()
 *
 * function_prefix:
 * the first part of the function name (eg. one of: amp_ rig_ rot_)
 * this is also the name of the member of the "self" object
 *
 * setting:
 * the last part of the function name (eg. func, level, ...)
 *
 * example: GENERATE_HAS_GET_SET(rig, func) creates the bindings for
 * rig_has_get_func() and rig_has_set_func()
 */
%define GENERATE_HAS_GET_SET(function_prefix, setting)
	setting_t has_get_##setting##(setting_t setting) {
		return function_prefix##_has_get_##setting##($self->##function_prefix##, setting);
	}

	setting_t has_set_##setting##(setting_t setting) {
		return function_prefix##_has_set_##setting##($self->##function_prefix##, setting);
	}
%enddef

/*
 * Macro AMP_GENERATE_HAS_GET_SET
 *
 * Creates two methods amp_has_get_*() and amp_has_set_*()
 *
 * setting:
 * the last part of the function name (eg. func, level, ...)
 *
 * example: AMP_GENERATE_HAS_GET_SET(level) creates the bindings for
 * amp_has_get_level() and amp_has_set_level()
 */
%define AMP_GENERATE_HAS_GET_SET(setting)
	GENERATE_HAS_GET_SET(amp, setting)
%enddef

/*
 * Macro ROT_GENERATE_HAS_GET_SET
 *
 * Creates two methods rot_has_get_*() and rot_has_set_*()
 *
 * setting:
 * the last part of the function name (eg. func, level, ...)
 *
 * example: ROT_GENERATE_HAS_GET_SET(func) creates the bindings for
 * rot_has_get_func() and rot_has_set_func()
 */
%define ROT_GENERATE_HAS_GET_SET(setting)
	GENERATE_HAS_GET_SET(rot, setting)
%enddef

/*
 * Macro GET_TOKEN
 *
 * function_prefix:
 * the first part of the function name (eg. one of: amp_ rig_ rot_)
 *
 * function_name:
 * the name that will be seen in the bindings and that creates a valid Hamlib function
 * name when appended to the function_prefix
 *
 * class_pointer:
 * the pointer to the instance of the class (eg. one of self->amp self->rig self->rot)
 */
%define GET_TOKEN(function_prefix, function_name, class_pointer)
PyObject * ##function_name(hamlib_token_t token)
{
	int value;

	self->error_status = ##function_prefix ##function_name(##class_pointer, token, &value);
	if (self->error_status != RIG_OK)
		return Py_None;

	return PyLong_FromLong(value);
}
%enddef

/*
 * Macro GET_VALUE_T
 *
 * function_prefix:
 * the first part of the function name (eg. one of: amp_ rig_ rot_)
 *
 * function_name:
 * the name that creates a valid Hamlib function name when appended to the
 * function_prefix
 *
 * class_pointer:
 * the pointer to the instance of the class (eg. one of self->amp self->rig self->rot)
 *
 * level_prefix:
 * the prefix of the macro that checks the datatype (eg. one of AMP_ RIG_ ROT_)
 */
%define GET_VALUE_T(function_prefix, function_name, class_pointer, level_prefix)
PyObject * ##function_name(hamlib_token_t token)
{
	value_t value;

	self->error_status = ##function_prefix ##function_name(##class_pointer, token, &value);
	if (self->error_status != RIG_OK)
		return Py_None;

#if defined(##level_prefix LEVEL_IS_FLOAT)
	if (##level_prefix LEVEL_IS_FLOAT(token))
		return PyFloat_FromDouble(value.f);
#endif

#if defined(##level_prefix LEVEL_IS_STRING)
	if (##level_prefix LEVEL_IS_STRING(token))
		return PyUnicode_FromString(value.s);
#endif

	return PyLong_FromLong(value.i);
}
%enddef

/*
* Macro AMP_GET_VALUE_T
*
* function_name:
* the name that will be seen in the bindings when appended to get_
* and that creates a valid Hamlib function when appended to amp_get_
* eg. AMP_GET_VALUE_T(level) creates Rot.get_level() that calls amp_get_level()
*/
%define AMP_GET_VALUE_T(function_name)
	GET_VALUE_T(amp_, get_ ##function_name, self->amp, AMP_)
%enddef

/*
 * Macro ROT_GET_LONG
 *
 * function_name:
 * the name that will be seen in the bindings when appended to get_
 * and that creates a valid Hamlib function when appended to rot_get_
 * eg. ROT_GET_LONG(func) creates Rot.get_func() that calls rot_get_func()
 */
%define ROT_GET_LONG(function_name)
	GET_TOKEN(rot_, get_ ##function_name, self->rot)
%enddef

/*
* Macro ROT_GET_VALUE_T
*
* function_name:
* the name that will be seen in the bindings when appended to get_
* and that creates a valid Hamlib function when appended to rot_get_
* eg. ROT_GET_VALUE_T(level) creates Rot.get_level() that calls rot_get_level()
*/
%define ROT_GET_VALUE_T(function_name)
	GET_VALUE_T(rot_, get_ ##function_name, self->rot, ROT_)
%enddef

/*
 * Macro SET_CALLBACK
 *
 * Creates a method that stores the Python arguments and sets the C callback.
 *
 * function_prefix:
 * the first part of the function name (eg. one of: amp_ rig_ rot_)
 *
 * class_pointer:
 * the pointer to the instance of the class (eg. one of self->amp self->rig self->rot)
 *
 * event_name:
 * the name that creates a valid Hamlib function name when used together
 * with the function_prefix and some predefined parts
 * eg. SET_CALLBACK("rig_", "freq") creates rig_set_freq_callback()
 */
%define SET_CALLBACK(function_prefix, class_pointer, event_name)
void set_ ## event_name ## _callback(PyObject *cb, PyObject *arg=NULL)
{
	void *callback;

	if (cb == Py_None) {
		callback = NULL;
	} else if (!PyCallable_Check(cb)) {
		SWIG_Python_RaiseOrModifyTypeError("The callback isn't a Python callable object");
		return;
	} else {
		callback = function_prefix ## event_name ## _cb_python;
	};

	Py_XINCREF(cb);
	Py_XDECREF(self->python_callbacks-> ## event_name ## _event);

	Py_XINCREF(arg);
	Py_XDECREF(self->python_callbacks-> ## event_name ## _arg);

	self->python_callbacks-> ## event_name ## _event = cb;
	self->python_callbacks-> ## event_name ## _arg = arg;
	self->error_status = function_prefix ## set_ ## event_name ## _callback(class_pointer, callback, self);

	return;
}
%enddef

/*
 * Macro RIG_SET_CALLBACK
 *
 * Creates a method that stores the Python arguments and sets the C callback.
 *
 * event_name:
 * the name that creates a valid Hamlib function name when used together
 * with the function_prefix and some predefined parts
 * eg. RIG_SET_CALLBACK("freq") creates rig_set_freq_callback()
 */
%define RIG_SET_CALLBACK(event_name)
	SET_CALLBACK(rig_, self->rig, event_name)
%enddef
