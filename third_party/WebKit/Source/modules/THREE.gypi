{
  'variables': {
    # Experimental hooks for embedder to provide extra IDL and source files.
    #
    # Note: this is not a supported API. If you rely on this, you will be broken
    # from time to time as the code generator changes in backward incompatible
    # ways.
    'THREE_modules_idl_files': [
        'THREE/THREE.idl',
        'THREE/Bindings/threeBlinkWrapper.idl',
        'THREE/Bindings/Matrix4B.idl',
    ],
    'THREE_modules_files': [
        'THREE/THREE.cpp',
        'THREE/THREE.h',
        'THREE/Bindings/threeBlinkWrapper.h',
        'THREE/Bindings/Matrix4B.h',
        'THREE/Bindings/Matrix4B.cpp',
        'THREE/Impl/threeBaseObj.h',
        'THREE/Impl/Matrix4.h',
        'THREE/Impl/Matrix4.cpp',
    ],
  },
}
