autocmd BufEnter wscript set sw=4 ts=4 sta noet fo=croql

let g:syntastic_mode_map = {
			\ "mode": "active",
			\ "active_filetypes": [],
			\ "passive_filetypes": []}

let g:syntastic_c_checkers = ['gcc']
let g:syntastic_c_compiler_options = "-std=c99 -Wall -Werror"
let g:syntastic_c_include_dirs = [
			\ "src/private",
			\ "third-party",
			\ "third-party/libuv/include"]

let g:syntastic_objc_compiler_options = g:syntastic_c_compiler_options
let g:syntastic_objc_include_dirs = g:syntastic_c_include_dirs

set path+=src/private,third-party,third-party/libuv/include
