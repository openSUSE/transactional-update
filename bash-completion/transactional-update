# transactional-update(8) completion              -*- shell-script -*-
#
# Copyright (C) 2023 SUSE Linux GmbH
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

__contains_word() {
    local w word="$1"
    shift
    for w in "$@"; do
        [[ $w = "$word" ]] && return 0
    done
    return 1
}

__get_snapshots() {
    ls -d /.snapshots/*/ 2>/dev/null | cut -d '/' -f 3 | sort --numeric-sort
}

__available_packages() {
    local lcur=$1
    [[ $lcur =~ / ]] && return
    set +o noglob
    grep -s --no-filename "^$lcur" /var/cache/zypp/solv/*/solv.idx | cut -f 1 | sort --unique
    set -o noglob
}

__installed_packages() {
    local lcur=$1
    [[ $lcur =~ / ]] && return
    grep -s --no-filename "^$lcur" "/var/cache/zypp/solv/@System/solv.idx" | cut -f 1
}

_transactional_update() {
    local i cmd comps
    local cur=${COMP_WORDS[COMP_CWORD]} prev=${COMP_WORDS[COMP_CWORD-1]} words cword
    local -A OPTS=(
        [STANDALONE]='--interactive -i
                      --non-interactive -n 
                      --no-selfupdate
                      --drop-if-no-change -d
                      --quiet'
        [STANDALONE_EXIT]='--help -h
                           --version'
        [ARG]='--continue -c'
    )
    local -A COMMANDS=(
        [STANDALONE]='cleanup
                      cleanup-snapshots
                      cleanup-overlays
                      grub.cfg
                      bootloader
                      initrd
                      kdump
                      shell
                      reboot
                      setup-selinux
                      dup
                      up
                      patch
                      migration
                      status'
        [ARG]='run
               setup-kdump
               pkg
               register
               rollback'
    )
    local CMD_PKG_OPTS='install in
                        remove rm
                        update up'

    _init_completion || return

    if __contains_word "$prev" ${OPTS[ARG]} ${COMMANDS[ARG]}; then
        case $prev in
            run)
                comps=$(compgen -c -- "$cur")
                compopt -o filenames
                ;;
            setup-kdump)
                comps="--crashkernel="
                ;;
            pkg)
                comps="${CMD_PKG_OPTS}"
                ;;
            rollback|--continue|-c)
                comps="last $(__get_snapshots)"
                ;;
        esac
        COMPREPLY=( $(compgen -W '$comps' -- "$cur") )
        return 0
    fi

    if __contains_word "$prev" ${OPTS[STANDALONE_EXIT]} ${COMMANDS[STANDALONE]} \
        && [[ "$prev" != "up" ]]; then
        # exception: 'up' is a standalone command and a pkg option
        return 0
    fi

    if [[ "$cur" = -* ]]; then
        comps="${OPTS[*]}"
    else
        comps="${OPTS[*]} ${COMMANDS[*]}"
    fi

    for ((i=0; i < COMP_CWORD; i++)); do
        if __contains_word "${COMP_WORDS[i]}" ${COMMANDS[*]} \
            && ! __contains_word "${COMP_WORDS[i-1]}" ${OPTS[ARG]}; then
            cmd=${COMP_WORDS[i]}
            break
        fi
    done
    if [[ -n ${cmd} ]]; then
        comps=""
        case $cmd in
            pkg)
                if __contains_word "$prev" ${CMD_PKG_OPTS}; then
                    case $prev in
                        install|in)
                            comps="$(__available_packages $cur)"
                            if [[ -z ${comps} ]]; then
                                # suggest file names to install local packages
                                comps=$(compgen -A file -- "$cur")
                                compopt -o filenames
                            fi
                            ;;
                        remove|rm|update|up)
                            comps="$(__installed_packages $cur)"
                            ;;
                    esac
                fi
                ;;
        esac
    fi

    COMPREPLY=( $(compgen -W '$comps' -- "$cur") )
    return 0
}

complete -F _transactional_update transactional-update
