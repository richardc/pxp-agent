{
  :type          => 'git',
  :forge_host    => 'forge-aio01-petest.puppetlabs.com',
  :load_path     => './lib/',
  :repo_proxy    => true,
  :add_el_extras => true,
  :'puppetserver-confdir' => '/etc/puppetserver/conf.d',
}
