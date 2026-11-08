import ctypes,sys,os,stat
A=ctypes.CDLL('libarchive.so.13')
A.archive_read_new.restype=ctypes.c_void_p
A.archive_read_support_filter_all.argtypes=[ctypes.c_void_p]
A.archive_read_support_format_all.argtypes=[ctypes.c_void_p]
A.archive_read_open_filename.argtypes=[ctypes.c_void_p,ctypes.c_char_p,ctypes.c_size_t]
A.archive_read_next_header.argtypes=[ctypes.c_void_p,ctypes.POINTER(ctypes.c_void_p)]
A.archive_entry_pathname.argtypes=[ctypes.c_void_p];A.archive_entry_pathname.restype=ctypes.c_char_p
A.archive_entry_size.argtypes=[ctypes.c_void_p];A.archive_entry_size.restype=ctypes.c_int64
A.archive_entry_filetype.argtypes=[ctypes.c_void_p];A.archive_entry_filetype.restype=ctypes.c_int
A.archive_read_data.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t];A.archive_read_data.restype=ctypes.c_ssize_t
A.archive_read_data_skip.argtypes=[ctypes.c_void_p]
A.archive_read_free.argtypes=[ctypes.c_void_p]
A.archive_error_string.argtypes=[ctypes.c_void_p];A.archive_error_string.restype=ctypes.c_char_p

def files(path,outdir=None):
 a=A.archive_read_new(); A.archive_read_support_filter_all(a);A.archive_read_support_format_all(a)
 rc=A.archive_read_open_filename(a,os.fsencode(path),10240)
 if rc<0: raise RuntimeError(A.archive_error_string(a))
 seen=[]; entry=ctypes.c_void_p()
 while (rc:=A.archive_read_next_header(a,ctypes.byref(entry)))==0:
  name=os.fsdecode(A.archive_entry_pathname(entry)); size=A.archive_entry_size(entry); typ=A.archive_entry_filetype(entry)
  seen.append((name,size,typ))
  if outdir and typ==stat.S_IFREG and not name.startswith('/') and '..' not in name.split('/'):
   target=os.path.join(outdir,name)
   os.makedirs(os.path.dirname(target),exist_ok=True)
   with open(target,'wb') as f:
    buf=ctypes.create_string_buffer(1<<17)
    while True:
     n=A.archive_read_data(a,buf,len(buf))
     if n==0: break
     if n<0: raise RuntimeError(f'{name}: '+str(A.archive_error_string(a)))
     f.write(buf.raw[:n])
  else: A.archive_read_data_skip(a)
 if rc != 1: print('ARCHIVE WARNING',A.archive_error_string(a),rc)
 A.archive_read_free(a)
 return seen

if __name__=='__main__':
 for f in sys.argv[1:]:
  v=files(f)
  print('\n'+f,'entries=',len(v),'bytes=',sum(max(x[1],0) for x in v))
  for row in v[:85]: print(row)
