namespace factory {

	namespace unix {

		struct proc_status {
			
			typedef int stat_type;

			constexpr bool
			exited() const noexcept {
				return WIFEXITED(stat);
			}

			constexpr bool
			signaled() const noexcept {
				return WIFSIGNALED(stat);
			}

			constexpr bool
			stopped() const noexcept {
				return WIFSTOPPED(stat);
			}
			
			constexpr stat_type
			exit_code() const noexcept {
				return WEXITSTATUS(stat);
			}
			
			constexpr stat_type
			term_signal() const noexcept {
				return WTERMSIG(stat);
			}
			
			constexpr stat_type
			stop_signal() const noexcept {
				return WSTOPSIG(stat);
			}
			
			constexpr bool
			core_dumped() const noexcept {
				return static_cast<bool>(WCOREDUMP(stat));
			}

			stat_type stat = 0;
		};

	}

}
