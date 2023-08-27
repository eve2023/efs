/*
   Copyright (c) 2023 Eve
   Licensed under the GNU General Public License version 3 (GPLv3)
*************************************************************************
   EVESFRUITLAND2.C
*************************************************************************
EFS is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any
later version.

EFS is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
details.

You should have received a copy of the GNU General Public License along
with EFS. If not, see <https://www.gnu.org/licenses/>.
*************************************************************************
*/
#include <signal.h>

void efs_serve(void);

int main(void)
{
  signal(SIGPIPE, SIG_IGN);

  efs_serve();

  return 0;
}
