_astraphosvc_completion() {
    local cur commands
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    commands="init version help"
    COMPREPLY=( $(compgen -W "$commands" -- "$cur") )
}
complete -F _astraphosvc_completion astraphosvc
