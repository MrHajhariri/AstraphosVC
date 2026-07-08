#compdef astraphosvc

_astraphosvc() {
  local -a commands
  commands=(
    'init:Initialize an AstraphosVC repository'
    'version:Print version information'
    'help:Print help text'
  )
  _describe 'command' commands
}

_astraphosvc "$@"
