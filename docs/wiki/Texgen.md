This documentation starts at the place where the texenv matrix is applied for `TEXGEN_NORMAL`, and works backward to explain where the values come from at each step.

### Function: `prepare_animated_material`

Executed starting at `0x02045D2C`.

* If the material's texgen mode is equal to `TEXGEN_NORMAL`:
  * If the value at memory location `0x020E3EA0` [→](#value-memory-location-0x020e3ea0) is greater than or equal to 0:
    * Set `texenv_mtx` to `current_node_transform` multiplied by `current_texture_matrix` [→](#variable-current_texture_matrix)
  * Else:
    * Set `texenv_mtx` to `current_node_transform`
  * Multiply the first column of `texenv_mtx` by `tex_width / 2.0` and multiply the second and third columns by `-(tex_height / 2.0)`
  * Set `temp_mtx` to `texenv_mtx` multiplied by the model texture matrix indexed with the material's `matrix_id` [→](#field-model-texture-matrices)
  * Set `texenv_mtx` to `temp_mtx` multiplied by `texcoord_mtx` [→](#variable-texcoord_mtx)
  * Execute `TEXCOORD` as `S = tex_width / 2.0` and `T = tex_height / 2.0`
  * Multiply the upper 3x4 of `texenv_mtx` by `16`, set `MTX_MODE` to `3`, and load the matrix

Notes:

* `texenv_mtx` is 4x4
* `current_node_transform` and `current_texture_matrix` are 4x3
* The model texture matrix is 4x4, but only the leftmost 4x2 elements are set and the rest is garbage data
  * As noted below, when this matrix is used, only the upper-left 3x2 elements are read
* Multiplication of `current_node_transform` and `current_texture_matrix` (or setting of `texenv_mtx` to `current_node_transform`) is done inline:
  * Only the upper-left 3x3 elements of `current_node_transform` are read
  * Only the upper-left 3x3 elements of `current_texture_matrix` are read
  * Only the upper-left 3x3 elements of `texenv_mtx` are written
* The last row of `texenv_mtx` is not multiplied by the texture width/height
* Multiplication of `texenv_mtx` with the model texture matrix and `texcoord_mtx` is done with a function call:
  * Only the upper-left 3x3 elements of the first matrix argument are read
  * Only the upper-left 3x2 elements of the second matrix argument are read
  * Only the upper-left 3x3 elements of the destination matrix are written

### Variable: `texcoord_mtx`

This is a `MtxFx44` variable used in `prepare_animated_material`. Set in `prepare_animated_material` starting at `0x02045C74`.

* If the material's texgen mode is not equal to `TEXGEN_NONE`:
  * If the model's texcoord animation pointer is set and the material's texcoord animation ID is not equal to `-1`:
    * Set `texcoord_mtx` to the result of `process_texcoord_animation`
  * Else, if the model's texture matrix pointer is set:
    * Set `texcoord_mtx` to the model texture matrix indexed with the material's `matrix_id`
  * Else:
    * Set `texcoord_mtx` based on the material's `scale_s`, `scale_t`, `rot_z`, `translate_s`, and `translate_t` fields

### Value: Memory location `0x020E3EA0`

This is an `int` value used in `prepare_animated_material`. Set in `draw_animated_model` starting at `0x02047158`.

* If the model's node animation pointer is set and bit 0 of `flags` [→](#variable-flags) is cleared:
  * Set to `-2147483648`
* Else:
  * Set to `0`

Note: While performing the Dialanche attack, the node animation pointer is nulled before the model is drawn, and then set back to normal.

### Variable: `flags`

This is a `byte` value passed to `draw_animated_model`.

* Depending on where `draw_animated_model` is called from, the value is either constant `0` or comes from the `some_flag` field of the `CModel` struct. In the latter case, none of the models that use normal texgen ever seem to change this flag, so it can be ignored.

### Variable: `current_texture_matrix`

This is a `MtxFx43` variable used in `prepare_animated_material` whose address is stored at memory location `0x27E2E10`. Set in `draw_animated_model` starting at `0x0204704C`.

* If `model->scale` is equal to `1.0`:
  * Set `temp_mtx` to `some_matrix` [→](#variable-some_matrix)
* Else:
  * Set `temp_mtx` to a scaling matrix of factor `model->scale` concatenated with `some_matrix`
* If bit 0 of `model->flags` [→](#field-model-flags) is set:
  * Set `current_texture_matrix` to `temp_mtx` concatenated with `view_matrix`
* Else:
  * Set `current_texture_matrix` to `temp_mtx`

### Field: `model->flags`

This is a `byte` field of the `Model` struct.

* Bit 0 appears to be set when any materials on the model use lighting, making the test of this flag in `current_texture_matrix` setup "does this model use lighting?" When the model uses lighting, the view matrix is included in the matrix setup; otherwise, it is not.

### Variable: `some_matrix`

This is a `MtxFx43` variable passed to `draw_animated_model`. For certain Hunter alt forms, set in `sub_2015ED4` using orientation vector fields on the `CPlayer` struct, and updated in `sub_201DCE4` based on player movement.

* TODO
* For the Morph Ball and Dialanche, this matrix is used in lieu of the node transform to hold the model's rotation.
* For items, this matrix holds the spinning/floating transform values.

### Field: Model texture matrices

* Hard coded for AlimbicCapsule. For other models, these are the texcoord transform matrices for materials, computed at load from each material's `scale_s`, `scale_t`, `rot_z`, `translate_s`, and `translate_t` fields.