/*
 * Copyright (C) 2015, 2016, 2017 "IoT.bzh"
 * Author José Bollo <jose.bollo@iot.bzh>
 * Author Ronan Le Martret <ronan.lemartret@iot.bzh>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define _GNU_SOURCE

#include "ExampleScene.h"
#include <functional>
#include <signal.h>

static bool running = true;

static void
handle_signal(int signum)
{
	running = false;
}

static void set_up_interrupt_handler() {
	struct sigaction sigint;
	sigint.sa_handler = handle_signal;
	sigemptyset(&sigint.sa_mask);
	sigint.sa_flags = SA_RESETHAND | SA_SIGINFO;
	sigaction(SIGINT, &sigint, NULL);
}

/* entry function */
int main(int ac, char **av, char **env)
{
	set_up_interrupt_handler();

	std::function<bool()> stillRunning = []() {
		return running;
	};

	ExampleScene *exampleScene = new ExampleScene();
	exampleScene->loop(stillRunning);
	fprintf(stderr, "done running.\n");
	delete exampleScene;
	return 0;
}
