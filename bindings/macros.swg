/*
Common SWIG macros
*/

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
